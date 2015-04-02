#ifndef p1_mac_plugins_display_stream_h
#define p1_mac_plugins_display_stream_h

#include "p1stream.h"
#include "module.h"

#include <CoreGraphics/CoreGraphics.h>

namespace p1_mac_plugins {


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


}  // namespace p1_mac_plugins

#endif  // p1_mac_plugins_display_stream.h
