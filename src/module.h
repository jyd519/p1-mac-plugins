#ifndef p1_mac_plugins_module_h
#define p1_mac_plugins_module_h

#define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED

#include "p1stream.h"

namespace p1_mac_plugins {


using namespace v8;
using namespace node;
using namespace p1stream;

extern Eternal<String> on_event_sym;
extern Eternal<String> display_id_sym;
extern Eternal<String> device_id_sym;
extern Eternal<String> server_id_sym;
extern Eternal<String> mixer_id_sym;
extern Eternal<String> divisor_sym;
extern Eternal<String> width_sym;
extern Eternal<String> height_sym;
extern Eternal<String> name_sym;
extern Eternal<String> app_sym;

extern Persistent<ObjectTemplate> hook_tmpl;

Local<String> v8_string_from_cf_string(Isolate *isolate, CFStringRef str);
CFStringRef cf_string_from_v8_string(Handle<Value> str);


}  // namespace p1_mac_plugins

#endif  // p1_mac_plugins_module.h
