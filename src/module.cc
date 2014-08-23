#include "mac_sources_priv.h"

namespace p1_mac_sources {


Eternal<String> display_id_sym;
Eternal<String> divisor_sym;
Eternal<String> device_sym;


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

static void audio_queue_constructor(const FunctionCallbackInfo<Value>& args)
{
    auto queue = new audio_queue();
    queue->init(args);
}

static void init(v8::Handle<v8::Object> exports, v8::Handle<v8::Value> module,
    v8::Handle<v8::Context> context, void* priv)
{
    auto *isolate = context->GetIsolate();
    Handle<String> name;
    Handle<FunctionTemplate> func;

#define SYM(handle, value) handle.Set(isolate, String::NewFromUtf8(isolate, value))
    SYM(display_id_sym, "displayId");
    SYM(divisor_sym, "divisor");
    SYM(device_sym, "device");
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
    display_link::init_prototype(func);
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
