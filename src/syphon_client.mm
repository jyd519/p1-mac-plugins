#include "syphon_client.h"

namespace p1_mac_plugins {

syphon_client::syphon_client() :
    buffer(this), client(nil)
{
}

void syphon_client::init(const FunctionCallbackInfo<Value>& args)
{
    auto *isolate = args.GetIsolate();
    Handle<Value> val;

    if (args.Length() != 1 || !args[0]->IsObject()) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Expected an object")));
        return;
    }
    auto params = args[0].As<Object>();

    val = params->Get(server_id_sym.Get(isolate));
    if (!val->IsString()) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Invalid server ID")));
        return;
    }

    NSString *uuid = CFBridgingRelease(cf_string_from_v8_string(val));
    if (uuid == nil) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Could not convert server ID to CFString")));
        return;
    }

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

    SyphonServerDirectory *directory = [SyphonServerDirectory sharedDirectory];
    for (NSDictionary *server in directory.servers) {
        if ([uuid isEqualToString:server[SyphonServerDescriptionUUIDKey]]) {
            client = [[SyphonClient alloc] initWithServerDescription:server
                                                             options:nil
                                                     newFrameHandler:nil];
            if (client != nil)
                break;
        }
    }
    if (client == nil)
        buffer.emitf(EV_LOG_ERROR, "Could not connect to server");
}

void syphon_client::destroy()
{
    if (client != nil) {
        [client stop];
        client = nil;
    }

    buffer.flush();

    Unref();
}

lockable *syphon_client::lock()
{
    return mutex.lock();
}

void syphon_client::produce_video_frame(video_source_context &ctx)
{
    IOSurfaceRef surface = client.surface;
    if (surface != NULL)
        ctx.render_iosurface(surface);
}

void syphon_client::init_prototype(Handle<FunctionTemplate> func)
{
    NODE_SET_PROTOTYPE_METHOD(func, "destroy", [](const FunctionCallbackInfo<Value>& args) {
        auto stream = ObjectWrap::Unwrap<syphon_client>(args.This());
        stream->destroy();
    });
}


}  // namespace p1_mac_plugins
