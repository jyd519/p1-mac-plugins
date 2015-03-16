#include "mac_plugins_priv.h"

namespace p1_mac_plugins {


Eternal<String> on_event_sym;
Eternal<String> display_id_sym;
Eternal<String> divisor_sym;
Eternal<String> device_sym;
Eternal<String> width_sym;
Eternal<String> height_sym;
Eternal<String> uid_sym;
Eternal<String> name_sym;
Eternal<String> mixer_id_sym;

Persistent<ObjectTemplate> hook_tmpl;


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

#define SYM(handle, value) handle.Set(isolate, String::NewFromUtf8(isolate, value))
    SYM(on_event_sym, "onEvent");
    SYM(display_id_sym, "displayId");
    SYM(divisor_sym, "divisor");
    SYM(device_sym, "device");
    SYM(width_sym, "width");
    SYM(height_sym, "height");
    SYM(uid_sym, "uid");
    SYM(name_sym, "name");
    SYM(mixer_id_sym, "mixerId");
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
