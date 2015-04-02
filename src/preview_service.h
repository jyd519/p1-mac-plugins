#ifndef p1_mac_plugins_preview_service_h
#define p1_mac_plugins_preview_service_h

#include "p1stream.h"
#include "p1stream_mac_preview.h"
#include "module.h"

namespace p1_mac_plugins {


#define EV_PREVIEW_REQUEST 'view'

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

#endif  // p1_mac_plugins_preview_service.h
