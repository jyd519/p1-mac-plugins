#include "mac_sources_priv.h"

namespace p1_mac_sources {


void display_stream::init(const FunctionCallbackInfo<Value>& args)
{
    bool ok = true;
    char err[128];

    CGError cg_ret;
    size_t width;
    size_t height;

    Handle<Object> params;
    Handle<Value> val;

    Wrap(args.This());
    args.GetReturnValue().Set(handle());
    auto isolate = args.GetIsolate();

    CGDirectDisplayID display_id = kCGDirectMainDisplay;

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
    }

    if (ok) {
        width  = CGDisplayPixelsWide(display_id);
        height = CGDisplayPixelsHigh(display_id);

        dispatch = dispatch_queue_create("display_stream", DISPATCH_QUEUE_SERIAL);
        if (!(ok = (dispatch != NULL)))
            strcpy(err, "dispatch_queue_create error");
    }

    if (ok) {
        cg_handle = CGDisplayStreamCreateWithDispatchQueue(
            display_id, width, height, 'BGRA', NULL, dispatch, ^(
                CGDisplayStreamFrameStatus status,
                uint64_t displayTime,
                IOSurfaceRef frameSurface,
                CGDisplayStreamUpdateRef updateRef)
            {
                this->callback(status, frameSurface);
            });
        if (!(ok = (cg_handle != NULL)))
            strcpy(err, "CGDisplayStreamCreateWithDispatchQueue error");
    }

    if (ok) {
        cg_ret = CGDisplayStreamStart(cg_handle);
        if (!(ok = (cg_ret == kCGErrorSuccess)))
            sprintf(err, "CGDisplayStreamStart error %d", cg_ret);
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

void display_stream::destroy(bool unref)
{
    CGError cg_ret;

    if (running) {
        cg_ret = CGDisplayStreamStop(cg_handle);
        if (cg_ret != kCGErrorSuccess)
            fprintf(stderr, "CGDisplayStreamStop error %d\n", cg_ret);
        running = false;
    }

    // FIXME: delay until stopped status callback

    if (cg_handle != NULL) {
        CFRelease(cg_handle);
        cg_handle = NULL;
    }

    if (dispatch != NULL) {
        dispatch_release(dispatch);
        dispatch = NULL;
    }

    if (unref)
        Unref();
}

void display_stream::produce_video_frame(video_source_context &ctx)
{
    lock_handle lock(mutex);

    if (last_frame != NULL)
        ctx.render_iosurface(last_frame);
}

void display_stream::callback(
    CGDisplayStreamFrameStatus status,
    IOSurfaceRef frame)
{
    lock_handle lock(mutex);

    // Ditch any previous frame, unless it's the same.
    // This also doubles as cleanup when stopping.
    if (last_frame != NULL && status != kCGDisplayStreamFrameStatusFrameIdle) {
        IOSurfaceDecrementUseCount(last_frame);
        CFRelease(last_frame);
        last_frame = NULL;
    }

    // A new frame arrived, retain it.
    if (status == kCGDisplayStreamFrameStatusFrameComplete) {
        last_frame = frame;
        CFRetain(last_frame);
        IOSurfaceIncrementUseCount(last_frame);
    }
}

void display_stream::init_prototype(Handle<FunctionTemplate> func)
{
    NODE_SET_PROTOTYPE_METHOD(func, "destroy", [](const FunctionCallbackInfo<Value>& args) {
        auto stream = ObjectWrap::Unwrap<display_stream>(args.This());
        stream->destroy();
    });
}


}  // namespace p1_mac_sources