#include "mac_sources_priv.h"

#include <bootstrap.h>

namespace p1_mac_sources {


static preview_service *singleton = nullptr;

void start_preview_service(const FunctionCallbackInfo<Value>& args)
{
    auto isolate = args.GetIsolate();

    if (args.Length() != 1 || !args[0]->IsFunction()) {
        isolate->ThrowException(Exception::TypeError(
                    String::NewFromUtf8(isolate, "Expected a function")));
        return;
    }

    if (singleton != nullptr) {
        isolate->ThrowException(Exception::Error(
                    String::NewFromUtf8(isolate, "Service already running")));
        return;
    }

    singleton = new preview_service();
    singleton->init(args.GetIsolate(), args[0].As<Function>());
}

void preview_service::init(Isolate *isolate_, Handle<Function> on_request_)
{
    num_pending = 0;

    isolate = isolate_;
    context.Reset(isolate, isolate->GetCurrentContext());

    on_request.Reset(isolate, on_request_);

    auto tmpl = ObjectTemplate::New(isolate);
    tmpl->SetInternalFieldCount(1);
    preview_client::init_template(tmpl);
    hook_template.Reset(isolate, tmpl);

    callback.init(std::bind(&preview_service::emit_pending, this));
    thread.init(std::bind(&preview_service::thread_loop, this));
}

void preview_service::thread_loop()
{
    mach_port_t service_port = get_service_port();
    if (service_port == MACH_PORT_NULL)
        return;

    do {
        // Block indefinitely until the next message.
        request_msg_rcv_t msg;
        mach_msg_return_t mret = mach_msg(
            &msg.header, MACH_RCV_MSG, 0, sizeof(msg), service_port,
            MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL
        );
        if (mret != MACH_MSG_SUCCESS) {
            fprintf(stderr, "mach_msg error 0x%x on service port\n", mret);
            break;
        }

        // Expect a non-complex message with our fixed size, containing send
        // rights. The mixer ID must be followed by all-zeroes, at least one.
        // This allows us to quickly check that it is null-terminated.
        bool ok =
            msg.header.msgh_id == p1_preview_request_msg_id &&
            msg.header.msgh_size == sizeof(msg) - sizeof(mach_msg_trailer_t) &&
            MACH_MSGH_BITS_REMOTE(msg.header.msgh_bits) == MACH_MSG_TYPE_PORT_SEND &&
            !(msg.header.msgh_bits & MACH_MSGH_BITS_COMPLEX) &&
            msg.mixer_id[127] == 0;

        // Check limit and queue up.
        if (ok) {
            lock_handle lock(*this);
            ok = num_pending != PREVIEW_MAX_PENDING;
            if (ok) {
                auto &entry = pending[num_pending++];
                strcpy(entry.mixer_id, msg.mixer_id);
                entry.client_port = msg.header.msgh_remote_port;
            }
        }

        // Notify main thead and clean up.
        if (ok)
            callback.send();
        else
            mach_msg_destroy(&msg.header);
    } while (true);

    // We never expect this to happen, and cannot really cleanup. But at least
    // close the service port, so other processes error.
    mach_port_destroy(mach_task_self(), service_port);
}

// Check-in with the bootstrap to acquired our receive port rights.
mach_port_t preview_service::get_service_port()
{
    kern_return_t kret;
    mach_port_t bootstrap_port;
    mach_port_t service_port = MACH_PORT_NULL;

    kret = task_get_bootstrap_port(mach_task_self(), &bootstrap_port);
    if (kret != KERN_SUCCESS) {
        fprintf(stderr, "task_get_bootstrap_port error 0x%x\n", kret);
    }
    else {
        kret = bootstrap_check_in(bootstrap_port, "com.p1stream.P1stream.preview", &service_port);
        if (kret != KERN_SUCCESS)
            fprintf(stderr, "bootstrap_check_in error 0x%x\n", kret);

        kret = mach_port_deallocate(mach_task_self(), bootstrap_port);
        if (kret != KERN_SUCCESS)
            fprintf(stderr, "mach_port_deallocate error 0x%x on bootstrap port\n", kret);
    }

    return service_port;
}

void preview_service::emit_pending()
{
    HandleScope handle_scope(isolate);
    Context::Scope context_scope(Local<Context>::New(isolate, context));

    preview_pending_client_t l_pending[PREVIEW_MAX_PENDING];
    size_t l_num_pending;

    // With lock, extract a copy of the buffer.
    {
        lock_handle lock(*this);

        if ((l_num_pending = num_pending) == 0)
            return;

        memcpy(l_pending, pending, l_num_pending * sizeof(preview_pending_client_t));
        num_pending = 0;
    }

    auto l_on_request = Local<Function>::New(isolate, on_request);
    auto l_hook_template = Local<ObjectTemplate>::New(isolate, hook_template);
    for (size_t i = 0; i < l_num_pending; i++) {
        auto &entry = l_pending[i];

        // Create the hook.
        auto str = String::NewFromUtf8(isolate, entry.mixer_id);
        auto obj = l_hook_template->NewInstance();
        auto client = new preview_client();
        client->init(isolate, obj, entry.client_port);

        // Callback.
        Handle<Value> args[2] = { str, obj };
        MakeCallback(isolate, obj, l_on_request, 2, args);
    }
}


void preview_client::init(Isolate *isolate_, Handle<Object> obj, mach_port_t client_port_)
{
    isolate = isolate_;
    context.Reset(isolate, isolate->GetCurrentContext());

    Wrap(obj);

    client_port = client_port_;

    callback.init(std::bind(&preview_client::emit_error, this));

    Ref();
}

void preview_client::destroy()
{
    close_port();

    context.Reset();

    Unref();
}

void preview_client::emit_error()
{
    HandleScope handle_scope(isolate);
    Context::Scope context_scope(Local<Context>::New(isolate, context));
    MakeCallback(isolate, handle(), "onClose", 0, NULL);
}

void preview_client::send_set_surface_msg(mach_port_t surface_port)
{
    set_surface_msg_send_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.header.msgh_size = sizeof(msg);
    msg.header.msgh_remote_port = client_port;
    msg.header.msgh_id = p1_preview_set_surface_msg_id;
    if (surface_port != MACH_PORT_NULL) {
        msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_COPY_SEND);
        msg.header.msgh_local_port = surface_port;
    }
    else {
        msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    }
    send_msg(&msg.header);
}

void preview_client::send_msg(mach_msg_header_t *msgh)
{
    if (client_port == MACH_PORT_NULL)
        return;

    mach_msg_return_t mret = mach_msg(
        msgh, MACH_SEND_MSG | MACH_SEND_TIMEOUT,
        msgh->msgh_size, 0, MACH_PORT_NULL, 0, MACH_PORT_NULL
    );
    if (mret != MACH_MSG_SUCCESS && mret != MACH_SEND_TIMED_OUT) {
        fprintf(stderr, "mach_msg error 0x%x on client port\n", mret);
        close_port();
        callback.send();
    }
}

void preview_client::close_port()
{
    if (client_port == MACH_PORT_NULL)
        return;

    kern_return_t kret = mach_port_deallocate(mach_task_self(), client_port);
    client_port = MACH_PORT_NULL;
    if (kret != KERN_SUCCESS)
        fprintf(stderr, "mach_port_deallocate error 0x%x on client port\n", kret);
}

bool preview_client::link_video_hook(video_hook_context &ctx)
{
    mach_port_t port = IOSurfaceCreateMachPort(ctx.mixer()->surface());
    send_set_surface_msg(port);

    kern_return_t kret = mach_port_deallocate(mach_task_self(), port);
    if (kret != KERN_SUCCESS)
        fprintf(stderr, "mach_port_deallocate error 0x%x on surface port\n", kret);

    return true;
}

void preview_client::unlink_video_hook(video_hook_context &ctx)
{
    send_set_surface_msg(MACH_PORT_NULL);
}

void preview_client::video_post_render(video_hook_context &ctx)
{
    updated_msg_send_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    msg.header.msgh_size = sizeof(msg);
    msg.header.msgh_remote_port = client_port;
    msg.header.msgh_id = p1_preview_updated_msg_id;
    send_msg(&msg.header);
}

void preview_client::init_template(Handle<ObjectTemplate> tmpl)
{
    NODE_SET_METHOD(tmpl, "destroy", [](const FunctionCallbackInfo<Value>& args) {
        auto client = ObjectWrap::Unwrap<preview_client>(args.This());
        client->destroy();
    });
}


}
