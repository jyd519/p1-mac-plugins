#include "detect_displays.h"

#include <CoreVideo/CoreVideo.h>

namespace p1_mac_plugins {

struct display_info {
    CGDirectDisplayID id;
    size_t width;
    size_t height;
};

static void reconfigure_callback(
   CGDirectDisplayID display,
   CGDisplayChangeSummaryFlags flags,
   void *userInfo);
static Local<Value> events_transform(
    Isolate *isolate, event &ev, buffer_slicer &slicer);
static Local<Value> display_infos_to_js(
    Isolate *isolate, display_info *infos,
    uint32_t count, buffer_slicer &slicer);


detect_displays::detect_displays() :
    buffer(this, events_transform), running(false)
{
}

void detect_displays::init(const FunctionCallbackInfo<Value>& args)
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

    auto cg_ret = CGDisplayRegisterReconfigurationCallback(reconfigure_callback, this);
    if (cg_ret != kCGErrorSuccess) {
        buffer.emitf(EV_LOG_ERROR, "CGDisplayRegisterReconfigurationCallback error 0x%x", cg_ret);
        return;
    }

    running = true;
    emit_change();
}

void detect_displays::destroy()
{
    if (running) {
        running = false;

        auto cg_ret = CGDisplayRemoveReconfigurationCallback(reconfigure_callback, this);
        if (cg_ret != kCGErrorSuccess)
            buffer.emitf(EV_LOG_ERROR, "CGDisplayRemoveReconfigurationCallback error 0x%x", cg_ret);
    }

    buffer.flush();

    Unref();
}

lockable *detect_displays::lock()
{
    return mutex.lock();
}

static void reconfigure_callback(
   CGDirectDisplayID display,
   CGDisplayChangeSummaryFlags flags,
   void *userInfo)
{
    // Ignore the begin pass.
    // FIXME: how to neatly emit only once per reconf here?
    if (flags != kCGDisplayBeginConfigurationFlag)
        ((detect_displays *) userInfo)->emit_change();
}

void detect_displays::emit_change()
{
    lock_handle lock(*this);

    CGError cg_ret;

    uint32_t count;
    cg_ret = CGGetOnlineDisplayList(0, NULL, &count);
    if (cg_ret != kCGErrorSuccess) {
        buffer.emitf(EV_LOG_ERROR, "CGGetOnlineDisplayList error 0x%x", cg_ret);
        return;
    }

    CGDirectDisplayID ids[count];
    cg_ret = CGGetOnlineDisplayList(count, ids, &count);
    if (cg_ret != kCGErrorSuccess) {
        buffer.emitf(EV_LOG_ERROR, "CGGetOnlineDisplayList error 0x%x", cg_ret);
        return;
    }

    auto *ev = buffer.emit(EV_DISPLAYS_CHANGED, count * sizeof(display_info));
    if (ev != NULL) {
        auto *infos = (display_info *) ev->data;
        for (uint32_t i = 0; i < count; i++) {
            auto &info = infos[i];
            auto id = ids[i];

            info.id = id;
            info.width = CGDisplayPixelsWide(id);
            info.height = CGDisplayPixelsHigh(id);
        }
    }
}

static Local<Value> events_transform(
    Isolate *isolate, event &ev, buffer_slicer &slicer)
{
    switch (ev.id) {
        case EV_DISPLAYS_CHANGED:
            return display_infos_to_js(
                isolate, (display_info *) ev.data,
                ev.size / sizeof(display_info), slicer
            );
        default:
            return Undefined(isolate);
    }
}

static Local<Value> display_infos_to_js(
    Isolate *isolate, display_info *infos,
    uint32_t count, buffer_slicer &slicer)
{
    auto l_display_id_sym = display_id_sym.Get(isolate);
    auto l_width_sym = width_sym.Get(isolate);
    auto l_height_sym = height_sym.Get(isolate);

    auto arr = Array::New(isolate, count);
    for (uint32_t i = 0; i < count; i++) {
        auto &info = infos[i];
        auto obj = Object::New(isolate);
        obj->Set(l_display_id_sym, Uint32::NewFromUnsigned(isolate, info.id));
        obj->Set(l_width_sym, Uint32::NewFromUnsigned(isolate, info.width));
        obj->Set(l_height_sym, Uint32::NewFromUnsigned(isolate, info.height));
        arr->Set(i, obj);
    }
    return arr;
}

void detect_displays::init_prototype(Handle<FunctionTemplate> func)
{
    NODE_SET_PROTOTYPE_METHOD(func, "destroy", [](const FunctionCallbackInfo<Value>& args) {
        auto detect = ObjectWrap::Unwrap<detect_displays>(args.This());
        detect->destroy();
    });
}


}  // namespace p1_mac_plugins
