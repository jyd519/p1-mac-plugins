#include "mac_sources_priv.h"

namespace p1_mac_sources {


Eternal<String> display_id_sym;
Eternal<String> divisor_sym;
Eternal<String> device_sym;
Eternal<String> width_sym;
Eternal<String> height_sym;
Eternal<String> on_change_sym;


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

static void init(Handle<Object> exports, Handle<Value> module,
    Handle<Context> context, void* priv)
{
    auto *isolate = context->GetIsolate();
    Handle<String> name;
    Handle<FunctionTemplate> func;

#define SYM(handle, value) handle.Set(isolate, String::NewFromUtf8(isolate, value))
    SYM(display_id_sym, "displayId");
    SYM(divisor_sym, "divisor");
    SYM(device_sym, "device");
    SYM(width_sym, "width");
    SYM(height_sym, "height");
    SYM(on_change_sym, "onChange");
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
}


} // namespace p1_mac_sources;

NODE_MODULE_CONTEXT_AWARE(native, p1_mac_sources::init)
