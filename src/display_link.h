#ifndef p1_mac_plugins_display_link_h
#define p1_mac_plugins_display_link_h

#include "p1stream.h"
#include "module.h"

#include <list>
#include <CoreVideo/CoreVideo.h>

namespace p1_mac_plugins {


#define EV_DISPLAY_LINK_STOPPED 'dstp'

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


}  // namespace p1_mac_plugins

#endif  // p1_mac_plugins_display_link.h
