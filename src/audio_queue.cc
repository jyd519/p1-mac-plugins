#include "mac_sources_priv.h"

namespace p1_mac_sources {


static const UInt32 num_channels = 2;
static const UInt32 sample_size = sizeof(float);
static const UInt32 sample_size_bits = sample_size * 8;
static const UInt32 sample_rate = 44100;

static void input_callback(
    void *inUserData,
    AudioQueueRef inAQ,
    AudioQueueBufferRef inBuffer,
    const AudioTimeStamp *inStartTime,
    UInt32 inNumberPacketDescriptions,
    const AudioStreamPacketDescription *inPacketDescs);



audio_queue::audio_queue() :
    buffer(this)
{
}

void audio_queue::init(const FunctionCallbackInfo<Value>& args)
{
    bool ok = true;
    OSStatus os_ret;
    auto *isolate = args.GetIsolate();
    Handle<Value> val;

    if (args.Length() != 1 || !args[0]->IsObject()) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Expected an object")));
        return;
    }
    auto params = args[0].As<Object>();

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

    AudioStreamBasicDescription fmt;
    fmt.mFormatID = kAudioFormatLinearPCM;
    fmt.mFormatFlags = kLinearPCMFormatFlagIsFloat;
    fmt.mSampleRate = sample_rate;
    fmt.mBitsPerChannel = sample_size_bits;
    fmt.mChannelsPerFrame = num_channels;
    fmt.mBytesPerFrame = num_channels * sample_size;
    fmt.mFramesPerPacket = 1;
    fmt.mBytesPerPacket = fmt.mBytesPerFrame;
    fmt.mReserved = 0;
    os_ret = AudioQueueNewInput(&fmt, input_callback, this, NULL, kCFRunLoopCommonModes, 0, &queue);
    if (!(ok = (os_ret == noErr)))
        buffer.emitf(EV_LOG_ERROR, "AudioQueueNewInput error 0x%x", os_ret);

    if (ok) {
        val = params->Get(device_sym.Get(isolate));
        if (!val->IsUndefined()) {
            String::Utf8Value strVal(val);
            if (!(ok = (*strVal != NULL))) {
                buffer.emitf(EV_LOG_ERROR, "Invalid device value");
            }
            else {
                CFStringRef str = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, *strVal, kCFStringEncodingUTF8, kCFAllocatorNull);
                if (!str)
                    abort();

                os_ret = AudioQueueSetProperty(queue, kAudioQueueProperty_CurrentDevice, &str, sizeof(str));
                CFRelease(str);
                if (!(ok = (os_ret == noErr)))
                    buffer.emitf(EV_LOG_ERROR, "AudioQueueSetProperty error 0x%x", os_ret);
            }
        }
    }

    if (ok) {
        for (UInt32 i = 0; i < num_buffers; i++) {
            os_ret = AudioQueueAllocateBuffer(queue, 0x5000, &buffers[i]);
            if (!(ok = (os_ret == noErr))) {
                buffer.emitf(EV_LOG_ERROR, "AudioQueueAllocateBuffer error 0x%x", os_ret);
                break;
            }

            os_ret = AudioQueueEnqueueBuffer(queue, buffers[i], 0, NULL);
            if (!(ok = (os_ret == noErr))) {
                buffer.emitf(EV_LOG_ERROR, "AudioQueueEnqueueBuffer error 0x%x", os_ret);
                AudioQueueFreeBuffer(queue, buffers[i]);
                break;
            }
        }
    }

    if (ok) {
        // Async, waits until running callback.
        os_ret = AudioQueueStart(queue, NULL);
        if (!(ok = (os_ret == noErr)))
            buffer.emitf(EV_LOG_ERROR, "AudioQueueStart error 0x%x", os_ret);
    }
}

void audio_queue::destroy()
{
    if (queue != NULL) {
        OSStatus ret = AudioQueueDispose(queue, FALSE);
        queue = NULL;
        if (ret != noErr)
            buffer.emitf(EV_LOG_ERROR, "AudioQueueDispose error 0x%x\n", ret);
    }

    buffer.flush();

    Unref();
}

lockable *audio_queue::lock()
{
    return mutex.lock();
}

void audio_queue::link_audio_source(audio_source_context &ctx)
{
    ctxes.push_back(&ctx);
}

void audio_queue::unlink_audio_source(audio_source_context &ctx)
{
    ctxes.remove(&ctx);
}

void input_callback(
    void *inUserData,
    AudioQueueRef inAQ,
    AudioQueueBufferRef inBuffer,
    const AudioTimeStamp *inStartTime,
    UInt32 inNumberPacketDescriptions,
    const AudioStreamPacketDescription *inPacketDescs)
{
    auto time = inStartTime->mHostTime;
    auto in = (float *)inBuffer->mAudioData;
    auto samples = inBuffer->mAudioDataByteSize / sample_size;
    auto &inst = *(audio_queue *) inUserData;

    lock_handle lock(inst);

    for (auto ctx : inst.ctxes)
        ctx->render_buffer(time, in, samples);

    OSStatus ret = AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);
    if (ret != noErr)
        inst.buffer.emitf(EV_LOG_ERROR, "AudioQueueEnqueueBuffer error 0x%x\n", ret);
}

void audio_queue::init_prototype(Handle<FunctionTemplate> func)
{
    NODE_SET_PROTOTYPE_METHOD(func, "destroy", [](const FunctionCallbackInfo<Value>& args) {
        auto link = ObjectWrap::Unwrap<audio_queue>(args.This());
        link->destroy();
    });
}


}  // namespace p1_mac_sources
