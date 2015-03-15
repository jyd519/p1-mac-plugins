#include "mac_plugins_priv.h"

#include <bootstrap.h>

namespace p1_mac_plugins {

struct request_msg_rcv_t {
    mach_msg_header_t header;
    char mixer_id[128];
    mach_msg_trailer_t trailer;
};

typedef mach_msg_empty_send_t set_surface_msg_send_t;

typedef mach_msg_empty_send_t updated_msg_send_t;

struct preview_request {
    char mixer_id[128];
    mach_port_t client_port;
    preview_service *service;
};

static Local<Value> events_transform(
    Isolate *isolate, event &ev, buffer_slicer &slicer);
static Local<Value> create_hook(Isolate *isolate, preview_request &req);

static preview_service *singleton = nullptr;


void preview_service::start(const FunctionCallbackInfo<Value>& args)
{
    if (singleton != nullptr) {
        auto isolate = args.GetIsolate();
        isolate->ThrowException(Exception::Error(
                    String::NewFromUtf8(isolate, "Service already running")));
        return;
    }

    singleton = new preview_service();
    singleton->init(args);
}


preview_service::preview_service() :
    buffer(this, events_transform)
{
}

void preview_service::init(const FunctionCallbackInfo<Value>& args)
{
    auto *isolate = args.GetIsolate();
    Handle<Value> val;

    if (args.Length() != 1 || !args[0]->IsObject()) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Expected an object")));
        return;
    }
    auto params = args[0].As<Object>();

    val = params->Get(name_sym.Get(isolate));
    if (!val->IsString()) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Expected a name string")));
        return;
    }

    String::Utf8Value v(val);
    if (*v == NULL) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Invalid name string")));
        return;
    }

    strcpy(name, *v);

    val = params->Get(on_event_sym.Get(isolate));
    if (!val->IsFunction()) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Expected an onEvent function")));
        return;
    }

    buffer.set_callback(isolate->GetCurrentContext(), val.As<Function>());

    thread.init(std::bind(&preview_service::thread_loop, this));
}

lockable *preview_service::lock()
{
    return mutex.lock();
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

            auto *ev = buffer.emit(EV_PREVIEW_REQUEST, sizeof(preview_request));
            if ((ok = (ev != NULL))) {
                auto &req = *(preview_request *) ev->data;
                strcpy(req.mixer_id, msg.mixer_id);
                req.client_port = msg.header.msgh_remote_port;
                req.service = this;
            }
        }

        // Clean up on bad message.
        if (!ok)
            mach_msg_destroy(&msg.header);
    } while (true);

    // FIXME: We never expect this to happen, and cannot really cleanup. But at
    // least close the service port, so other processes error.
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
        buffer.emitf(EV_LOG_ERROR, "task_get_bootstrap_port error 0x%x", kret);
    }
    else {
        kret = bootstrap_check_in(bootstrap_port, name, &service_port);
        if (kret != KERN_SUCCESS)
            buffer.emitf(EV_LOG_ERROR, "bootstrap_check_in error 0x%x", kret);

        kret = mach_port_deallocate(mach_task_self(), bootstrap_port);
        if (kret != KERN_SUCCESS)
            buffer.emitf(EV_LOG_ERROR, "mach_port_deallocate error 0x%x on bootstrap port", kret);
    }

    return service_port;
}

static Local<Value> events_transform(
    Isolate *isolate, event &ev, buffer_slicer &slicer)
{
    switch (ev.id) {
        case EV_PREVIEW_REQUEST:
            return create_hook(isolate, *(preview_request *) ev.data);
        default:
            return Undefined(isolate);
    }
}

static Local<Value> create_hook(Isolate *isolate, preview_request &req)
{
    auto l_hook_tmpl = Local<ObjectTemplate>::New(isolate, hook_tmpl);

    // Create the hook object.
    auto obj = l_hook_tmpl->NewInstance();
    obj->Set(mixer_id_sym.Get(isolate), String::NewFromUtf8(isolate, req.mixer_id));

    // Create the hook wrap.
    auto client = new preview_client(*req.service);
    client->init(isolate, obj, req.client_port);

    return obj;
}


preview_client::preview_client(preview_service &service_) :
    error_async(std::bind(&preview_client::emit_client_error, this)),
    service(service_)
{
}

void preview_client::init(Isolate *isolate_, Handle<Object> obj, mach_port_t client_port_)
{
    isolate = isolate_;
    context.Reset(isolate, isolate->GetCurrentContext());

    Wrap(obj);

    client_port = client_port_;

    Ref();
}

void preview_client::destroy()
{
    close_port();

    context.Reset();

    Unref();
}

void preview_client::emit_client_error()
{
    HandleScope handle_scope(isolate);
    if (!context.IsEmpty()) {
        Context::Scope context_scope(Local<Context>::New(isolate, context));
        MakeCallback(isolate, handle(), "onClose", 0, NULL);
    }
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
        lock_handle lock(service);
        if (mret == MACH_SEND_INVALID_DEST)
            service.buffer.emitf(EV_LOG_INFO, "Preview client went away");
        else
            service.buffer.emitf(EV_LOG_WARN, "mach_msg error 0x%x on client port", mret);

        close_port();
        error_async.signal();
    }
}

void preview_client::close_port()
{
    if (client_port == MACH_PORT_NULL)
        return;

    kern_return_t kret = mach_port_deallocate(mach_task_self(), client_port);
    client_port = MACH_PORT_NULL;
    if (kret != KERN_SUCCESS) {
        lock_handle lock(service);
        service.buffer.emitf(EV_LOG_ERROR, "mach_port_deallocate error 0x%x on client port", kret);
    }
}

void preview_client::link_video_hook(video_hook_context &ctx)
{
    mach_port_t port = IOSurfaceCreateMachPort(ctx.mixer()->surface());
    send_set_surface_msg(port);

    kern_return_t kret = mach_port_deallocate(mach_task_self(), port);
    if (kret != KERN_SUCCESS) {
        lock_handle lock(service);
        service.buffer.emitf(EV_LOG_ERROR, "mach_port_deallocate error 0x%x on surface port", kret);
    }
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
