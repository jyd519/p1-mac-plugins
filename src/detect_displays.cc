#include "mac_sources_priv.h"

namespace p1_mac_sources {


void detect_displays::init(const FunctionCallbackInfo<Value>& args)
{
    bool ok;
    char err[128];

    CGError cg_ret;

    Handle<Object> params;
    Handle<Value> val;

    Wrap(args.This());
    args.GetReturnValue().Set(handle());
    isolate = args.GetIsolate();

    if (!(ok = (args.Length() == 1)))
        strcpy(err, "Expected one argument");
    else if (!(ok = args[0]->IsObject()))
        strcpy(err, "Expected an object");
    else
        params = args[0].As<Object>();

    if (ok) {
        val = params->Get(on_change_sym.Get(isolate));
        if (!(ok = val->IsFunction()))
            strcpy(err, "Invalid or missing onChange handler");
    }

    if (ok) {
        on_change.Reset(isolate, val.As<Function>());

        cg_ret = CGDisplayRegisterReconfigurationCallback(
                reconfigure_callback, this);
        if (!(ok = (cg_ret == kCGErrorSuccess)))
            sprintf(err, "CGDisplayRegisterReconfigurationCallback error 0x%x", cg_ret);
    }

    if (ok) {
        callback.init(std::bind(&detect_displays::emit_change, this));

        Ref();

        emit_change();
    }
    else {
        destroy(false);
        isolate->ThrowException(Exception::Error(
                    String::NewFromUtf8(isolate, err)));
    }
}

void detect_displays::destroy(bool unref)
{
    CGDisplayRemoveReconfigurationCallback(reconfigure_callback, this);

    callback.destroy();

    on_change.Reset();

    if (unref)
        Unref();
}

void detect_displays::emit_change()
{
    CGError cg_ret;

    uint32_t count;
    cg_ret = CGGetOnlineDisplayList(0, NULL, &count);
    if (cg_ret != kCGErrorSuccess) {
        fprintf(stderr, "CGGetOnlineDisplayList error 0x%x", cg_ret);
        return;
    }

    uint32_t ids[count];
    cg_ret = CGGetOnlineDisplayList(count, ids, &count);
    if (cg_ret != kCGErrorSuccess) {
        fprintf(stderr, "CGGetOnlineDisplayList error 0x%x", cg_ret);
        return;
    }

    auto arr = Array::New(isolate, count);
    for (uint32_t i = 0; i < count; i++) {
        uint32_t id = ids[i];

        auto obj = Object::New(isolate);
        arr->Set(i, obj);

        obj->Set(display_id_sym.Get(isolate),
                Uint32::New(isolate, id));
        obj->Set(width_sym.Get(isolate),
                Uint32::New(isolate, CGDisplayPixelsWide(id)));
        obj->Set(height_sym.Get(isolate),
                Uint32::New(isolate, CGDisplayPixelsHigh(id)));
    }

    auto l_on_change = Local<Function>::New(isolate, on_change);
    Handle<Value> arg = arr;
    MakeCallback(isolate, handle(isolate), l_on_change, 1, &arg);
}

void detect_displays::reconfigure_callback(
   CGDirectDisplayID display,
   CGDisplayChangeSummaryFlags flags,
   void *userInfo)
{
    // Ignore the begin pass.
    if (flags & kCGDisplayBeginConfigurationFlag)
        return;

    // Signal the main loop, which won't run until the after pass finishes.
    ((detect_displays *) userInfo)->callback.send();
}

void detect_displays::init_prototype(Handle<FunctionTemplate> func)
{
    NODE_SET_PROTOTYPE_METHOD(func, "destroy", [](const FunctionCallbackInfo<Value>& args) {
        auto detect = ObjectWrap::Unwrap<detect_displays>(args.This());
        detect->destroy();
    });
}


}  // namespace p1_mac_sources
