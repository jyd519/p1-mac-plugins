#include "syphon_directory.h"

@interface syphon_directory_observer : NSObject
- (id)initWithParent:(p1_mac_plugins::syphon_directory *)parent;
@end

namespace p1_mac_plugins {

struct server_info {
    CFStringRef uuid;
    CFStringRef name;
    CFStringRef app;
    // FIXME: icon
};


static Local<Value> events_transform(
    Isolate *isolate, event &ev, buffer_slicer &slicer);
static Local<Value> server_infos_to_js(
    Isolate *isolate, server_info *infos,
    uint32_t count, buffer_slicer &slicer);


syphon_directory::syphon_directory() :
    buffer(this, events_transform), observer(nil)
{
}

void syphon_directory::init(const FunctionCallbackInfo<Value>& args)
{
    auto *isolate = args.GetIsolate();
    Handle<Value> val;

    if (args.Length() != 1 || !args[0]->IsObject()) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Expected an object")));
        return;
    }
    auto params = args[0].As<Object>();

    val = params->Get(on_event_sym.Get(isolate));
    if (!val->IsFunction()) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Expected an onEvent function")));
        return;
    }

    // Parameters checked, from here on we no longer throw exceptions.
    Wrap(args.This());
    Ref();
    args.GetReturnValue().Set(handle());

    buffer.set_callback(isolate->GetCurrentContext(), val.As<Function>());

    observer = [[syphon_directory_observer alloc] initWithParent:this];
    directory = [SyphonServerDirectory sharedDirectory];
    [directory addObserver:observer
                forKeyPath:@"servers"
                   options:NSKeyValueObservingOptionNew | NSKeyValueObservingOptionInitial
                   context:NULL];
}

void syphon_directory::destroy()
{
    if (observer != nil) {
        [directory removeObserver:observer forKeyPath:@"servers"];
        directory = nil;
        observer = nil;
    }

    buffer.flush();

    Unref();
}

lockable *syphon_directory::lock()
{
    return mutex.lock();
}

void syphon_directory::emit_change()
{
    NSArray *servers = directory.servers;
    NSUInteger count = servers.count;

    auto *ev = buffer.emit(EV_SYPHON_SERVERS_CHANGED, count * sizeof(server_info));
    if (ev == NULL)
        return;

    auto *infos = (server_info *) ev->data;
    for (NSUInteger i = 0; i < count; i++) {
        auto &info = infos[i];
        NSDictionary *server = servers[i];

        info.uuid = (CFStringRef) CFBridgingRetain(server[SyphonServerDescriptionUUIDKey]);
        info.name = (CFStringRef) CFBridgingRetain(server[SyphonServerDescriptionNameKey]);
        info.app  = (CFStringRef) CFBridgingRetain(server[SyphonServerDescriptionAppNameKey]);
    }
}

static Local<Value> events_transform(
    Isolate *isolate, event &ev, buffer_slicer &slicer)
{
    switch (ev.id) {
        case EV_SYPHON_SERVERS_CHANGED:
            return server_infos_to_js(
                isolate, (server_info *) ev.data,
                ev.size / sizeof(server_info), slicer
            );
        default:
            return Undefined(isolate);
    }
}

static Local<Value> server_infos_to_js(
    Isolate *isolate, server_info *infos,
    uint32_t count, buffer_slicer &slicer)
{
    auto l_server_id_sym = server_id_sym.Get(isolate);
    auto l_name_sym = name_sym.Get(isolate);
    auto l_app_sym = app_sym.Get(isolate);

    auto arr = Array::New(isolate, count);
    for (uint32_t i = 0; i < count; i++) {
        auto &info = infos[i];

        auto obj = Object::New(isolate);
        obj->Set(l_server_id_sym, v8_string_from_cf_string(isolate, info.uuid));
        obj->Set(l_name_sym, v8_string_from_cf_string(isolate, info.name));
        obj->Set(l_app_sym, v8_string_from_cf_string(isolate, info.app));
        arr->Set(i, obj);

        CFRelease(info.uuid);
        CFRelease(info.name);
        CFRelease(info.app);
    }
    return arr;
}

void syphon_directory::init_prototype(Handle<FunctionTemplate> func)
{
    NODE_SET_PROTOTYPE_METHOD(func, "destroy", [](const FunctionCallbackInfo<Value>& args) {
        auto detect = ObjectWrap::Unwrap<syphon_directory>(args.This());
        detect->destroy();
    });
}


}  // namespace p1_mac_plugins


@implementation syphon_directory_observer {
    p1_mac_plugins::syphon_directory *parent;
}

- (id)initWithParent:(p1_mac_plugins::syphon_directory *)parent_
{
    self = [super init];
    if (self != nil) {
        parent = parent_;
    }
    return self;
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context
{
    parent->emit_change();
}

@end
