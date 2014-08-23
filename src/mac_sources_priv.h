#ifndef p1_mac_sources_priv_h
#define p1_mac_sources_priv_h

#include "p1stream.h"

#include <CoreVideo/CoreVideo.h>
#include <AudioToolbox/AudioToolbox.h>

namespace p1_mac_sources {

using namespace v8;
using namespace node;
using namespace p1stream;


extern Eternal<String> display_id_sym;
extern Eternal<String> divisor_sym;
extern Eternal<String> device_sym;


class display_link : public video_clock {
private:
    Isolate *isolate;
    CVDisplayLinkRef cv_handle;
    lockable_mutex mutex;
    bool running;
    uint32_t divisor;
    uint32_t skip_counter;

    video_clock_context *ctx;

    static CVReturn callback(
        CVDisplayLinkRef display_link,
        const CVTimeStamp *now,
        const CVTimeStamp *output_time,
        CVOptionFlags flags_in,
        CVOptionFlags *flags_out,
        void *context);
    void tick(frame_time_t time);

public:
    // Public JavaScript methods.
    void init(const FunctionCallbackInfo<Value>& args);
    void destroy(bool unref = true);

    // Lockable implementation.
    virtual lockable *lock() final;

    // Video clock implementation.
    virtual bool link_video_clock(video_clock_context &ctx_) final;
    virtual void unlink_video_clock(video_clock_context &ctx_) final;
    virtual fraction_t video_ticks_per_second(video_clock_context &ctx) final;

    // Module init.
    static void init_prototype(Handle<FunctionTemplate> func);
};


class display_stream : public video_source {
private:
    dispatch_queue_t dispatch;
    CGDisplayStreamRef cg_handle;
    lockable_mutex mutex;
    bool running;

    IOSurfaceRef last_frame;

    void callback(
        CGDisplayStreamFrameStatus status,
        IOSurfaceRef frame);

public:
    // Public JavaScript methods.
    void init(const FunctionCallbackInfo<Value>& args);
    void destroy(bool unref = true);

    // Video source implementation.
    virtual void produce_video_frame(video_source_context &ctx) final;

    // Module init.
    static void init_prototype(Handle<FunctionTemplate> func);
};


class audio_queue : public audio_source {
private:
    static const UInt32 num_buffers = 3;

    Isolate *isolate;
    AudioQueueRef queue;
    AudioQueueBufferRef buffers[num_buffers];

    audio_source_context *ctx;

    static void input_callback(
        void *inUserData,
        AudioQueueRef inAQ,
        AudioQueueBufferRef inBuffer,
        const AudioTimeStamp *inStartTime,
        UInt32 inNumberPacketDescriptions,
        const AudioStreamPacketDescription *inPacketDescs);

public:
    // Public JavaScript methods.
    void init(const FunctionCallbackInfo<Value>& args);
    void destroy(bool unref = true);

    // Audio source implementation.
    virtual bool link_audio_source(audio_source_context &ctx) final;
    virtual void unlink_audio_source(audio_source_context &ctx) final;

    // Module init.
    static void init_prototype(Handle<FunctionTemplate> func);
};


}  // namespace p1_mac_sources

#endif  // p1_mac_sources_priv_h