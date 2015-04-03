#ifndef p1_mac_plugins_syphon_client_h
#define p1_mac_plugins_syphon_client_h

#include "p1stream.h"
#include "module.h"

#import <Syphon/Syphon.h>

namespace p1_mac_plugins {


class syphon_client : public video_source, public lockable {
public:
    syphon_client();

    lockable_mutex mutex;
    event_buffer buffer;

    SyphonClient *client;

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

#endif  // p1_mac_plugins_syphon_client.h
