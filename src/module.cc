#include "audio_queue.h"
#include "detect_audio_inputs.h"
#include "detect_displays.h"
#include "display_link.h"
#include "display_stream.h"
#include "preview_service.h"

#include <CoreFoundation/CoreFoundation.h>

namespace p1_mac_plugins {


Eternal<String> on_event_sym;
Eternal<String> display_id_sym;
Eternal<String> device_id_sym;
Eternal<String> mixer_id_sym;
Eternal<String> divisor_sym;
Eternal<String> width_sym;
Eternal<String> height_sym;
Eternal<String> name_sym;

Persistent<ObjectTemplate> hook_tmpl;


Local<String> v8_string_from_cf_string(Isolate *isolate, CFStringRef str)
{
    auto len = CFStringGetLength(str);
    auto *ptr = CFStringGetCharactersPtr(str);
    if (ptr != NULL) {
        return String::NewFromTwoByte(isolate, ptr, String::kNormalString, len);
    }
    else {
        UniChar buf[len];
        CFStringGetCharacters(str, CFRangeMake(0, len), buf);
        return String::NewFromTwoByte(isolate, buf, String::kNormalString, len);
    }
}

CFStringRef cf_string_from_v8_string(Handle<Value> str)
{
    String::Value val(str);
    return CFStringCreateWithCharacters(kCFAllocatorDefault, *val, val.length());
}

static void display_link_constructor(const FunctionCallbackInfo<Value>& args)
{
    auto link = new display_link();
    link->init(args);
}

static void display_stream_constructor(const FunctionCallbackInfo<Value>& args)
{
    auto stream = new display_stream();
    stream->init(args);
}

static void detect_displays_constructor(const FunctionCallbackInfo<Value>& args)
{
    auto detect = new detect_displays();
    detect->init(args);
}

static void audio_queue_constructor(const FunctionCallbackInfo<Value>& args)
{
    auto queue = new audio_queue();
    queue->init(args);
}

static void detect_audio_inputs_constructor(const FunctionCallbackInfo<Value>& args)
{
    auto detect = new detect_audio_inputs();
    detect->init(args);
}

static void init(Handle<Object> exports, Handle<Value> module,
    Handle<Context> context, void* priv)
{
    auto *isolate = context->GetIsolate();
    Handle<String> name;
    Handle<FunctionTemplate> func;

    NODE_DEFINE_CONSTANT(exports, EV_DISPLAYS_CHANGED);
    NODE_DEFINE_CONSTANT(exports, EV_AUDIO_INPUTS_CHANGED);
    NODE_DEFINE_CONSTANT(exports, EV_PREVIEW_REQUEST);
    NODE_DEFINE_CONSTANT(exports, EV_AQ_IS_RUNNING);
    NODE_DEFINE_CONSTANT(exports, EV_DISPLAY_LINK_STOPPED);
    NODE_DEFINE_CONSTANT(exports, EV_SYPHON_SERVERS_CHANGED);

#define SYM(handle, value) handle.Set(isolate, String::NewFromUtf8(isolate, value))
    SYM(on_event_sym, "onEvent");
    SYM(display_id_sym, "displayId");
    SYM(device_id_sym, "deviceId");
    SYM(mixer_id_sym, "mixerId");
    SYM(divisor_sym, "divisor");
    SYM(width_sym, "width");
    SYM(height_sym, "height");
    SYM(name_sym, "name");
#undef SYM

    name = String::NewFromUtf8(isolate, "DisplayLink");
    func = FunctionTemplate::New(isolate, display_link_constructor);
    func->InstanceTemplate()->SetInternalFieldCount(1);
    func->SetClassName(name);
    display_link::init_prototype(func);
    exports->Set(name, func->GetFunction());

    name = String::NewFromUtf8(isolate, "DisplayStream");
    func = FunctionTemplate::New(isolate, display_stream_constructor);
    func->InstanceTemplate()->SetInternalFieldCount(1);
    func->SetClassName(name);
    display_stream::init_prototype(func);
    exports->Set(name, func->GetFunction());

    name = String::NewFromUtf8(isolate, "DetectDisplays");
    func = FunctionTemplate::New(isolate, detect_displays_constructor);
    func->InstanceTemplate()->SetInternalFieldCount(1);
    func->SetClassName(name);
    detect_displays::init_prototype(func);
    exports->Set(name, func->GetFunction());

    name = String::NewFromUtf8(isolate, "AudioQueue");
    func = FunctionTemplate::New(isolate, audio_queue_constructor);
    func->InstanceTemplate()->SetInternalFieldCount(1);
    func->SetClassName(name);
    audio_queue::init_prototype(func);
    exports->Set(name, func->GetFunction());

    name = String::NewFromUtf8(isolate, "DetectAudioInputs");
    func = FunctionTemplate::New(isolate, detect_audio_inputs_constructor);
    func->InstanceTemplate()->SetInternalFieldCount(1);
    func->SetClassName(name);
    detect_audio_inputs::init_prototype(func);
    exports->Set(name, func->GetFunction());

    name = String::NewFromUtf8(isolate, "startPreviewService");
    func = FunctionTemplate::New(isolate, preview_service::start);
    exports->Set(name, func->GetFunction());

    auto tmpl = ObjectTemplate::New(isolate);
    tmpl->SetInternalFieldCount(1);
    preview_client::init_template(tmpl);
    hook_tmpl.Reset(isolate, tmpl);
}


} // namespace p1_mac_plugins;

NODE_MODULE_CONTEXT_AWARE(native, p1_mac_plugins::init)
