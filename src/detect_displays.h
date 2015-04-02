#ifndef p1_mac_plugins_detect_displays_h
#define p1_mac_plugins_detect_displays_h

#include "p1stream.h"
#include "module.h"

namespace p1_mac_plugins {


#define EV_DISPLAYS_CHANGED 'disp'

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


}  // namespace p1_mac_plugins

#endif  // p1_mac_plugins_detect_displays.h
