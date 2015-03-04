#include "mac_sources_priv.h"

namespace p1_mac_sources {

static CVReturn display_link_callback(
    CVDisplayLinkRef cv_handle,
    const CVTimeStamp *now,
    const CVTimeStamp *output_time,
    CVOptionFlags flags_in,
    CVOptionFlags *flags_out,
    void *context);


display_link::display_link() :
    buffer(this), cv_handle(NULL), running(false), skip_counter(0)
{
}

void display_link::init(const FunctionCallbackInfo<Value>& args)
{
    auto *isolate = args.GetIsolate();
    Handle<Value> val;
    CGDirectDisplayID display_id = kCGDirectMainDisplay;

    if (args.Length() != 1 || !args[0]->IsObject()) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Expected an object")));
        return;
    }
    auto params = args[0].As<Object>();

    val = params->Get(display_id_sym.Get(isolate));
    if (val->IsUint32()) {
        display_id = val->Uint32Value();
    }
    else if (val->IsUndefined()) {
        display_id = kCGDirectMainDisplay;
    }
    else {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Invalid display ID")));
        return;
    }

    val = params->Get(divisor_sym.Get(isolate));
    if (val->IsUint32())
        divisor = val->Uint32Value();
    else if (val->IsUndefined())
        divisor = 1;
    else
        divisor = 0;

    if (divisor == 0) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Invalid divisor value")));
        return;
    }

    val = params->Get(on_event_sym.Get(isolate));
    if (!val->IsFunction()) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Expected an onEvent function")));
        return;
    }

    // Parameters checked, from here on we no longer throw exceptions.
    Wrap(args.This());
    Ref();
    args.GetReturnValue().Set(handle());

    buffer.set_callback(isolate->GetCurrentContext(), val.As<Function>());

    CVReturn cv_ret;

    cv_ret = CVDisplayLinkCreateWithCGDisplay(display_id, &cv_handle);
    if (cv_ret != kCVReturnSuccess) {
        buffer.emitf(EV_LOG_ERROR, "CVDisplayLinkCreateWithCGDisplay error 0x%x", cv_ret);
        return;
    }

    cv_ret = CVDisplayLinkSetOutputCallback(cv_handle, display_link_callback, this);
    if (cv_ret != kCVReturnSuccess) {
        buffer.emitf(EV_LOG_ERROR, "CVDisplayLinkSetOutputCallback error 0x%x", cv_ret);
        return;
    }

    cv_ret = CVDisplayLinkStart(cv_handle);
    if (cv_ret != kCVReturnSuccess) {
        buffer.emitf(EV_LOG_ERROR, "CVDisplayLinkStart error 0x%x", cv_ret);
        return;
    }

    running = true;
}

void display_link::destroy()
{
    if (running) {
        running = false;

        auto cv_ret = CVDisplayLinkStop(cv_handle);
        if (cv_ret != kCVReturnSuccess)
            buffer.emitf(EV_LOG_ERROR, "CVDisplayLinkStop error 0x%x\n", cv_ret);
    }

    if (cv_handle != NULL) {
        CFRelease(cv_handle);
        cv_handle = NULL;
    }

    buffer.flush();

    Unref();
}

lockable *display_link::lock()
{
    return mutex.lock();
}

void display_link::link_video_clock(video_clock_context &ctx)
{
    ctxes.push_back(&ctx);
}

void display_link::unlink_video_clock(video_clock_context &ctx)
{
    ctxes.remove(&ctx);
}

fraction_t display_link::video_ticks_per_second(video_clock_context &ctx)
{
    double period = CVDisplayLinkGetActualOutputVideoRefreshPeriod(cv_handle);
    if (period == 0.0) {
        return {
            .num = 0,
            .den = 0
        };
    }
    else {
        return {
            .num = (uint32_t) round(1.0 / period),
            .den = divisor
        };
    }
}

static CVReturn display_link_callback(
    CVDisplayLinkRef cv_handle,
    const CVTimeStamp *now,
    const CVTimeStamp *output_time,
    CVOptionFlags flags_in,
    CVOptionFlags *flags_out,
    void *context)
{
    auto &link = *(display_link *) context;

    // Skip tick based on divisor.
    if (link.skip_counter == link.divisor)
        link.skip_counter = 0;
    if (link.skip_counter++ != 0)
        return kCVReturnSuccess;

    // Call mixer with lock.
    {
        lock_handle lock(link);
        auto time = now->hostTime;
        for (auto ctx : link.ctxes)
            ctx->tick(time);
    }

    return kCVReturnSuccess;
}

void display_link::init_prototype(Handle<FunctionTemplate> func)
{
    NODE_SET_PROTOTYPE_METHOD(func, "destroy", [](const FunctionCallbackInfo<Value>& args) {
        auto link = ObjectWrap::Unwrap<display_link>(args.This());
        link->destroy();
    });
}


}  // namespace p1_mac_sources
