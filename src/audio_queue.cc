#include "mac_sources_priv.h"

namespace p1_mac_sources {


static const UInt32 num_channels = 2;
static const UInt32 sample_size = sizeof(float);
static const UInt32 sample_size_bits = sample_size * 8;
static const UInt32 sample_rate = 44100;


void audio_queue::init(const FunctionCallbackInfo<Value>& args)
{
    bool ok = true;
    char err[128];

    OSStatus os_ret;

    Handle<Object> params;
    Handle<Value> val;

    Wrap(args.This());
    args.GetReturnValue().Set(handle());
    isolate = args.GetIsolate();

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

    if (args.Length() == 1) {
        if (!(ok = args[0]->IsObject()))
            strcpy(err, "Expected an object");
        else
            params = Local<Object>::Cast(args[0]);
    }

    if (ok) {
        os_ret = AudioQueueNewInput(&fmt, input_callback, this, NULL, kCFRunLoopCommonModes, 0, &queue);
        if (!(ok = (os_ret == noErr)))
            sprintf(err, "AudioQueueNewInput error 0x%x", os_ret);
    }

    if (ok && !params.IsEmpty()) {
        val = params->Get(device_sym.Get(isolate));
        if (!val->IsUndefined()) {
            String::Utf8Value strVal(val);
            if (!(ok = (*strVal != NULL))) {
                strcpy(err, "Invalid device value");
            }
            else {
                CFStringRef str = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, *strVal, kCFStringEncodingUTF8, kCFAllocatorNull);
                if (!str)
                    abort();

                os_ret = AudioQueueSetProperty(queue, kAudioQueueProperty_CurrentDevice, &str, sizeof(str));
                CFRelease(str);
                if (!(ok = (os_ret == noErr)))
                    sprintf(err, "AudioQueueSetProperty error 0x%x", os_ret);
            }
        }
    }

    if (ok) {
        for (UInt32 i = 0; i < num_buffers; i++) {
            os_ret = AudioQueueAllocateBuffer(queue, 0x5000, &buffers[i]);
            if (!(ok = (os_ret == noErr))) {
                sprintf(err, "AudioQueueAllocateBuffer error 0x%x", os_ret);
                break;
            }

            os_ret = AudioQueueEnqueueBuffer(queue, buffers[i], 0, NULL);
            if (!(ok = (os_ret == noErr))) {
                sprintf(err, "AudioQueueEnqueueBuffer error 0x%x", os_ret);
                AudioQueueFreeBuffer(queue, buffers[i]);
                break;
            }
        }
    }

    if (ok) {
        // Async, waits until running callback.
        os_ret = AudioQueueStart(queue, NULL);
        if (!(ok = (os_ret == noErr)))
            sprintf(err, "AudioQueueStart error 0x%x", os_ret);
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

void audio_queue::destroy(bool unref)
{
    if (queue != NULL) {
        OSStatus ret = AudioQueueDispose(queue, FALSE);
        queue = NULL;
        if (ret != noErr)
            fprintf(stderr, "AudioQueueDispose error 0x%x\n", ret);
    }

    if (unref)
        Unref();
}

void audio_queue::link_audio_source(audio_source_context &ctx)
{
    ctxes.push_back(&ctx);
}

void audio_queue::unlink_audio_source(audio_source_context &ctx)
{
    ctxes.remove(&ctx);
}

void audio_queue::input_callback(
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

    for (auto ctx : ((audio_queue *) inUserData)->ctxes)
        ctx->render_buffer(time, in, samples);

    OSStatus ret = AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);
    if (ret != noErr)
        fprintf(stderr, "AudioQueueEnqueueBuffer error 0x%x\n", ret);
}

void audio_queue::init_prototype(Handle<FunctionTemplate> func)
{
    NODE_SET_PROTOTYPE_METHOD(func, "destroy", [](const FunctionCallbackInfo<Value>& args) {
        auto link = ObjectWrap::Unwrap<audio_queue>(args.This());
        link->destroy();
    });
}


}  // namespace p1_mac_sources
