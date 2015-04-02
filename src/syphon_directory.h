#ifndef p1_mac_plugins_syphon_directory_h
#define p1_mac_plugins_syphon_directory_h

#include "p1stream.h"
#include "module.h"

#import <Syphon/Syphon.h>

namespace p1_mac_plugins {


#define EV_SYPHON_SERVERS_CHANGED 'sphs'

class syphon_directory : public ObjectWrap, public lockable {
public:
    syphon_directory();

    lockable_mutex mutex;
    event_buffer buffer;

    SyphonServerDirectory *directory;
    id observer;

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


}  // namespace p1_mac_plugins

#endif  // p1_mac_plugins_syphon_directory.h
