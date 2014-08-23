#include "mac_sources_priv.h"

namespace p1_mac_sources {


void display_link::init(const FunctionCallbackInfo<Value>& args)
{
    bool ok = true;
    char err[128];

    CVReturn cv_ret;

    Handle<Object> params;
    Handle<Value> val;

    Wrap(args.This());
    args.GetReturnValue().Set(handle());
    isolate = args.GetIsolate();

    CGDirectDisplayID display_id = kCGDirectMainDisplay;
    divisor = 1;

    if (args.Length() == 1) {
        if (!(ok = args[0]->IsObject()))
            strcpy(err, "Expected an object");
        else
            params = Local<Object>::Cast(args[0]);

        if (ok) {
            val = params->Get(display_id_sym.Get(isolate));
            if (!val->IsUndefined()) {
                if (!(ok = val->IsUint32()))
                    strcpy(err, "Invalid display ID");
                else
                    display_id = val->Uint32Value();
            }
        }

        if (ok) {
            val = params->Get(divisor_sym.Get(isolate));
            if (!val->IsUndefined()) {
                if (val->IsUint32())
                    divisor = val->Uint32Value();
                else
                    divisor = 0;

                if (!(ok = (divisor >= 1)))
                    strcpy(err, "Invalid divisor value");
            }
        }
    }

    if (ok) {
        cv_ret = CVDisplayLinkCreateWithCGDisplay(display_id, &cv_handle);
        if (!(ok = (cv_ret == kCVReturnSuccess)))
            sprintf(err, "CVDisplayLinkCreateWithCGDisplay error %d", cv_ret);
    }

    if (ok) {
        cv_ret = CVDisplayLinkSetOutputCallback(cv_handle, callback, this);
        if (!(ok = (cv_ret == kCVReturnSuccess)))
            sprintf(err, "CVDisplayLinkSetOutputCallback error %d", cv_ret);
    }

    if (ok) {
        cv_ret = CVDisplayLinkStart(cv_handle);
        if (!(ok = (cv_ret == kCVReturnSuccess)))
            sprintf(err, "CVDisplayLinkStart error %d", cv_ret);
        else
            running = true;
    }

    if (ok) {
        Ref();
    }
    else {
        destroy(false);
        isolate->ThrowException(Exception::Error(
                    String::NewFromUtf8(isolate, err)));
    }
}

void display_link::destroy(bool unref)
{
    CVReturn cv_ret;

    if (running) {
        cv_ret = CVDisplayLinkStop(cv_handle);
        if (cv_ret != kCVReturnSuccess)
            fprintf(stderr, "CVDisplayLinkStop error %d\n", cv_ret);
        running = false;
    }

    if (cv_handle != NULL) {
        CFRelease(cv_handle);
        cv_handle = NULL;
    }

    if (unref)
        Unref();
}

lockable *display_link::lock()
{
    return mutex.lock();
}

bool display_link::link_video_clock(video_clock_context &ctx_)
{
    if (ctx != nullptr) {
        isolate->ThrowException(Exception::Error(String::NewFromUtf8(isolate,
            "DisplayLink can only link to one mixer")));
        return false;
    }

    ctx = &ctx_;
    return true;
}

void display_link::unlink_video_clock(video_clock_context &ctx_)
{
    if (ctx == &ctx_)
        ctx = nullptr;
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

CVReturn display_link::callback(
    CVDisplayLinkRef cv_handle,
    const CVTimeStamp *now,
    const CVTimeStamp *output_time,
    CVOptionFlags flags_in,
    CVOptionFlags *flags_out,
    void *context)
{
    auto *link = (display_link *) context;
    link->tick(now->hostTime);
    return kCVReturnSuccess;
}

void display_link::tick(frame_time_t time)
{
    // Skip tick based on divisor.
    if (skip_counter == divisor)
        skip_counter = 0;
    if (skip_counter++ != 0)
        return;

    // Call mixer with lock.
    {
        lock_handle lock(mutex);
        if (ctx != nullptr)
            ctx->tick(time);
    }
}

void display_link::init_prototype(Handle<FunctionTemplate> func)
{
    NODE_SET_PROTOTYPE_METHOD(func, "destroy", [](const FunctionCallbackInfo<Value>& args) {
        auto link = ObjectWrap::Unwrap<display_link>(args.This());
        link->destroy();
    });
}


}  // namespace p1_mac_sources
