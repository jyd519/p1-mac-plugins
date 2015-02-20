#ifndef p1_mac_sources_priv_h
#define p1_mac_sources_priv_h

#include "p1stream.h"
#include "p1stream_mac_preview.h"

#include <list>
#include <CoreVideo/CoreVideo.h>
#include <AudioToolbox/AudioToolbox.h>

namespace p1_mac_sources {

using namespace v8;
using namespace node;
using namespace p1stream;


extern Eternal<String> display_id_sym;
extern Eternal<String> divisor_sym;
extern Eternal<String> device_sym;
extern Eternal<String> width_sym;
extern Eternal<String> height_sym;
extern Eternal<String> on_change_sym;


class display_link : public video_clock {
public:
    Isolate *isolate;
    CVDisplayLinkRef cv_handle;
    lockable_mutex mutex;
    bool running;
    uint32_t divisor;
    uint32_t skip_counter;

    std::list<video_clock_context *> ctxes;

    static CVReturn callback(
        CVDisplayLinkRef display_link,
        const CVTimeStamp *now,
        const CVTimeStamp *output_time,
        CVOptionFlags flags_in,
        CVOptionFlags *flags_out,
        void *context);
    void tick(frame_time_t time);

    // Public JavaScript methods.
    void init(const FunctionCallbackInfo<Value>& args);
    void destroy(bool unref = true);

    // Lockable implementation.
    virtual lockable *lock() final;

    // Video clock implementation.
    virtual bool link_video_clock(video_clock_context &ctx) final;
    virtual void unlink_video_clock(video_clock_context &ctx) final;
    virtual fraction_t video_ticks_per_second(video_clock_context &ctx) final;

    // Module init.
    static void init_prototype(Handle<FunctionTemplate> func);
};


class display_stream : public video_source {
public:
    dispatch_queue_t dispatch;
    CGDisplayStreamRef cg_handle;
    lockable_mutex mutex;
    bool running;

    IOSurfaceRef last_frame;

    void callback(
        CGDisplayStreamFrameStatus status,
        IOSurfaceRef frame);

    // Public JavaScript methods.
    void init(const FunctionCallbackInfo<Value>& args);
    void destroy(bool unref = true);

    // Video source implementation.
    virtual void produce_video_frame(video_source_context &ctx) final;

    // Module init.
    static void init_prototype(Handle<FunctionTemplate> func);
};


class detect_displays : public ObjectWrap {
public:
    Isolate *isolate;
    Persistent<Function> on_change;
    main_loop_callback callback;

    static void reconfigure_callback(
       CGDirectDisplayID display,
       CGDisplayChangeSummaryFlags flags,
       void *userInfo);
    void emit_change();

    // Public JavaScript methods.
    void init(const FunctionCallbackInfo<Value>& args);
    void destroy(bool unref = true);

    // Module init.
    static void init_prototype(Handle<FunctionTemplate> func);
};


class audio_queue : public audio_source {
public:
    static const UInt32 num_buffers = 3;

    Isolate *isolate;
    AudioQueueRef queue;
    AudioQueueBufferRef buffers[num_buffers];

    std::list<audio_source_context *> ctxes;

    static void input_callback(
        void *inUserData,
        AudioQueueRef inAQ,
        AudioQueueBufferRef inBuffer,
        const AudioTimeStamp *inStartTime,
        UInt32 inNumberPacketDescriptions,
        const AudioStreamPacketDescription *inPacketDescs);

    // Public JavaScript methods.
    void init(const FunctionCallbackInfo<Value>& args);
    void destroy(bool unref = true);

    // Audio source implementation.
    virtual bool link_audio_source(audio_source_context &ctx) final;
    virtual void unlink_audio_source(audio_source_context &ctx) final;

    // Module init.
    static void init_prototype(Handle<FunctionTemplate> func);
};


#define PREVIEW_MAX_PENDING 4

struct request_msg_rcv_t {
    mach_msg_header_t header;
    char mixer_id[128];
    mach_msg_trailer_t trailer;
};

typedef mach_msg_empty_send_t set_surface_msg_send_t;

typedef mach_msg_empty_send_t updated_msg_send_t;

struct preview_pending_client_t {
    char mixer_id[128];
    mach_port_t client_port;
};

class preview_service : public lockable_mutex {
private:
    Isolate *isolate;
    Persistent<Context> context;
    Persistent<Function> on_request;
    Persistent<ObjectTemplate> hook_template;

    threaded_loop thread;
    void thread_loop();
    mach_port_t get_service_port();

    main_loop_callback callback;
    preview_pending_client_t pending[PREVIEW_MAX_PENDING];
    size_t num_pending;
    void emit_pending();

public:
    void init(Isolate *isolate_, Handle<Function> on_request_);
};

class preview_client : public video_hook {
private:
    Isolate *isolate;
    Persistent<Context> context;

    mach_port_t client_port;

    main_loop_callback callback;
    void emit_error();

    void send_set_surface_msg(mach_port_t surface_port);
    void send_msg(mach_msg_header_t *msgh);
    void close_port();

public:
    // Public JavaScript methods.
    void init(Isolate *isolate_, Handle<Object> obj, mach_port_t client_port_);
    void destroy();

    // Video hook implementation.
    virtual bool link_video_hook(video_hook_context &ctx) final;
    virtual void unlink_video_hook(video_hook_context &ctx) final;
    virtual void video_post_render(video_hook_context &ctx) final;

    // Module init.
    static void init_template(Handle<ObjectTemplate> tmpl);
};

void start_preview_service(const FunctionCallbackInfo<Value>& args);


}  // namespace p1_mac_sources

#endif  // p1_mac_sources_priv_h
