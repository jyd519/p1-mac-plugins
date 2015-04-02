#ifndef p1_mac_plugins_audio_queue_h
#define p1_mac_plugins_audio_queue_h

#include "p1stream.h"
#include "module.h"

#include <list>
#include <AudioToolbox/AudioToolbox.h>

namespace p1_mac_plugins {


#define EV_AQ_IS_RUNNING 'qrun'

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


}  // namespace p1_mac_plugins

#endif  // p1_mac_plugins_audio_queue_h
