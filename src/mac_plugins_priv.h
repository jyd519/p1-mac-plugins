#ifndef p1_mac_plugins_priv_h
#define p1_mac_plugins_priv_h

#include "p1stream.h"
#include "p1stream_mac_preview.h"

#include <list>
#include <CoreVideo/CoreVideo.h>
#include <AudioToolbox/AudioToolbox.h>

namespace p1_mac_plugins {

#define EV_DISPLAYS_CHANGED 'disp'
#define EV_AUDIO_INPUTS_CHANGED 'ainp'
#define EV_PREVIEW_REQUEST 'view'
#define EV_AQ_IS_RUNNING 'qrun'
#define EV_DISPLAY_LINK_STOPPED 'dstp'

using namespace v8;
using namespace node;
using namespace p1stream;


extern Eternal<String> on_event_sym;
extern Eternal<String> display_id_sym;
extern Eternal<String> divisor_sym;
extern Eternal<String> device_id_sym;
extern Eternal<String> width_sym;
extern Eternal<String> height_sym;
extern Eternal<String> name_sym;
extern Eternal<String> mixer_id_sym;

extern Persistent<ObjectTemplate> hook_tmpl;


class display_link : public video_clock {
public:
    display_link();

    lockable_mutex mutex;
    event_buffer buffer;

    uint32_t divisor;

    CVDisplayLinkRef cv_handle;
    bool running;
    uint32_t skip_counter;

    std::list<video_clock_context *> ctxes;

    // Public JavaScript methods.
    void init(const FunctionCallbackInfo<Value>& args);
    void stop();
    void destroy();

    // Lockable implementation.
    virtual lockable *lock() final;

    // Video clock implementation.
    virtual void link_video_clock(video_clock_context &ctx) final;
    virtual void unlink_video_clock(video_clock_context &ctx) final;
    virtual fraction_t video_ticks_per_second(video_clock_context &ctx) final;

    // Module init.
    static void init_prototype(Handle<FunctionTemplate> func);
};


class display_stream : public video_source, public lockable {
public:
    display_stream();

    lockable_mutex mutex;
    event_buffer buffer;

    dispatch_queue_t dispatch;
    CGDisplayStreamRef cg_handle;
    bool running;

    IOSurfaceRef last_frame;

    // Public JavaScript methods.
    void init(const FunctionCallbackInfo<Value>& args);
    void destroy();

    // Lockable implementation.
    virtual lockable *lock() final;

    // Video source implementation.
    virtual void produce_video_frame(video_source_context &ctx) final;

    // Module init.
    static void init_prototype(Handle<FunctionTemplate> func);
};


class detect_displays : public ObjectWrap, public lockable {
public:
    detect_displays();

    lockable_mutex mutex;
    event_buffer buffer;

    bool running;

    // Internal.
    void emit_change();

    // Public JavaScript methods.
    void init(const FunctionCallbackInfo<Value>& args);
    void destroy();

    // Lockable implementation.
    virtual lockable *lock() final;

    // Module init.
    static void init_prototype(Handle<FunctionTemplate> func);
};


class audio_queue : public audio_source, public lockable {
public:
    static const UInt32 num_buffers = 3;

    audio_queue();

    lockable_mutex mutex;
    event_buffer buffer;

    std::list<audio_source_context *> ctxes;

    AudioQueueRef queue;
    AudioQueueBufferRef buffers[num_buffers];

    // Public JavaScript methods.
    void init(const FunctionCallbackInfo<Value>& args);
    void stop();
    void destroy();

    // Lockable implementation.
    virtual lockable *lock() final;

    // Audio source implementation.
    virtual void link_audio_source(audio_source_context &ctx) final;
    virtual void unlink_audio_source(audio_source_context &ctx) final;

    // Module init.
    static void init_prototype(Handle<FunctionTemplate> func);
};


class detect_audio_inputs : public ObjectWrap, public lockable {
public:
    detect_audio_inputs();

    lockable_mutex mutex;
    event_buffer buffer;

    bool running;

    // Internal.
    void emit_change();

    // Public JavaScript methods.
    void init(const FunctionCallbackInfo<Value>& args);
    void destroy();

    // Lockable implementation.
    virtual lockable *lock() final;

    // Module init.
    static void init_prototype(Handle<FunctionTemplate> func);
};


class preview_service : public lockable {
public:
    preview_service();

    lockable_mutex mutex;
    event_buffer buffer;

    char name[BOOTSTRAP_MAX_NAME_LEN];
    Persistent<ObjectTemplate> hook_template;

    threaded_loop thread;
    void thread_loop();

    // Internal.
    mach_port_t get_service_port();

    // Public JavaScript methods.
    void init(const FunctionCallbackInfo<Value>& args);
    static void start(const FunctionCallbackInfo<Value>& args);

    // Lockable implementation.
    virtual lockable *lock() final;
};

class preview_client : public video_hook {
public:
    preview_client(preview_service &service_);

    async error_async;

    Isolate *isolate;
    Persistent<Context> context;

    preview_service &service;
    mach_port_t client_port;

    void send_set_surface_msg(mach_port_t surface_port);
    void send_msg(mach_msg_header_t *msgh);
    void close_port();
    void emit_client_error();

    // Public JavaScript methods.
    void init(Isolate *isolate_, Handle<Object> obj, mach_port_t client_port_);
    void destroy();

    // Video hook implementation.
    virtual void link_video_hook(video_hook_context &ctx) final;
    virtual void unlink_video_hook(video_hook_context &ctx) final;
    virtual void video_post_render(video_hook_context &ctx) final;

    // Module init.
    static void init_template(Handle<ObjectTemplate> tmpl);
};


}  // namespace p1_mac_plugins

#endif  // p1_mac_plugins_priv_h
