#include "mac_sources_priv.h"

namespace p1_mac_sources {

static void display_stream_callback(
    display_stream &stream,
    CGDisplayStreamFrameStatus status,
    IOSurfaceRef frame);


display_stream::display_stream() :
    buffer(this), dispatch(NULL), cg_handle(NULL), running(false), last_frame(NULL)
{
}

void display_stream::init(const FunctionCallbackInfo<Value>& args)
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

    CGError cg_ret;

    size_t width  = CGDisplayPixelsWide(display_id);
    size_t height = CGDisplayPixelsHigh(display_id);

    dispatch = dispatch_queue_create("display_stream", DISPATCH_QUEUE_SERIAL);
    if (dispatch == NULL) {
        buffer.emitf(EV_LOG_ERROR, "dispatch_queue_create error");
        return;
    }

    cg_handle = CGDisplayStreamCreateWithDispatchQueue(
        display_id, width, height, 'BGRA', NULL, dispatch, ^(
            CGDisplayStreamFrameStatus status,
            uint64_t displayTime,
            IOSurfaceRef frameSurface,
            CGDisplayStreamUpdateRef updateRef)
        {
            display_stream_callback(*this, status, frameSurface);
        });
    if (cg_handle == NULL) {
        buffer.emitf(EV_LOG_ERROR, "CGDisplayStreamCreateWithDispatchQueue error");
        return;
    }

    cg_ret = CGDisplayStreamStart(cg_handle);
    if (cg_ret != kCGErrorSuccess) {
        buffer.emitf(EV_LOG_ERROR, "CGDisplayStreamStart error 0x%x", cg_ret);
        return;
    }

    running = true;
}

void display_stream::destroy()
{
    if (running) {
        running = false;

        auto cg_ret = CGDisplayStreamStop(cg_handle);
        if (cg_ret != kCGErrorSuccess)
            buffer.emitf(EV_LOG_ERROR, "CGDisplayStreamStop error 0x%x\n", cg_ret);
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

    buffer.flush();

    Unref();
}

lockable *display_stream::lock()
{
    return mutex.lock();
}

void display_stream::produce_video_frame(video_source_context &ctx)
{
    lock_handle lock(*this);

    if (last_frame != NULL)
        ctx.render_iosurface(last_frame);
}

static void display_stream_callback(
    display_stream &stream,
    CGDisplayStreamFrameStatus status,
    IOSurfaceRef frame)
{
    lock_handle lock(stream);

    // Ditch any previous frame, unless it's the same.
    // This also doubles as cleanup when stopping.
    if (stream.last_frame != NULL && status != kCGDisplayStreamFrameStatusFrameIdle) {
        IOSurfaceDecrementUseCount(stream.last_frame);
        CFRelease(stream.last_frame);
        stream.last_frame = NULL;
    }

    // A new frame arrived, retain it.
    if (status == kCGDisplayStreamFrameStatusFrameComplete) {
        stream.last_frame = frame;
        CFRetain(stream.last_frame);
        IOSurfaceIncrementUseCount(stream.last_frame);
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
