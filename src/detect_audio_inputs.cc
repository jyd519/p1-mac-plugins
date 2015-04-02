#include "detect_audio_inputs.h"

#include <AudioToolbox/AudioToolbox.h>

namespace p1_mac_plugins {

struct audio_input_info {
    CFStringRef uid;
    CFStringRef name;
};

static OSStatus property_callback(
    AudioObjectID inObjectID, UInt32 inNumberAddresses,
    const AudioObjectPropertyAddress inAddresses[], void *inClientData);
static Local<Value> events_transform(
    Isolate *isolate, event &ev, buffer_slicer &slicer);
static Local<Value> audio_input_infos_to_js(
    Isolate *isolate, audio_input_info *infos,
    uint32_t count, buffer_slicer &slicer);

static const AudioObjectPropertyAddress system_devices_addr = {
    kAudioHardwarePropertyDevices,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
};

static const AudioObjectPropertyAddress device_streams_addr = {
    kAudioDevicePropertyStreams,
    kAudioObjectPropertyScopeInput,
    kAudioObjectPropertyElementMaster
};

static const AudioObjectPropertyAddress device_uid_addr = {
    kAudioDevicePropertyDeviceUID,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
};

static const AudioObjectPropertyAddress object_name_addr = {
    kAudioObjectPropertyName,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster
};


detect_audio_inputs::detect_audio_inputs() :
    buffer(this, events_transform), running(false)
{
}

void detect_audio_inputs::init(const FunctionCallbackInfo<Value>& args)
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

    auto ret = AudioObjectAddPropertyListener(
        kAudioObjectSystemObject, &system_devices_addr,
        property_callback, this);
    if (ret != noErr) {
        buffer.emitf(EV_LOG_ERROR, "AudioObjectAddPropertyListener error 0x%x", ret);
        return;
    }

    running = true;
    emit_change();
}

void detect_audio_inputs::destroy()
{
    if (running) {
        running = false;

        auto ret = AudioObjectRemovePropertyListener(
            kAudioObjectSystemObject, &system_devices_addr,
            property_callback, this);
        if (ret != noErr)
            buffer.emitf(EV_LOG_ERROR, "AudioObjectRemovePropertyListener error 0x%x", ret);
    }

    buffer.flush();

    Unref();
}

lockable *detect_audio_inputs::lock()
{
    return mutex.lock();
}

static OSStatus property_callback(
    AudioObjectID inObjectID, UInt32 inNumberAddresses,
    const AudioObjectPropertyAddress inAddresses[], void *inClientData)
{
    // Check to see if the device list changed.
    if (inObjectID == kAudioObjectSystemObject) {
        for (UInt32 i = 0; i < inNumberAddresses; i++) {
            if (inAddresses[i].mSelector == kAudioHardwarePropertyDevices) {
                ((detect_audio_inputs *) inClientData)->emit_change();
                break;
            }
        }
    }
    return noErr;
}

void detect_audio_inputs::emit_change()
{
    lock_handle lock(*this);

    OSStatus ret;

    // Get the number of devices.
    UInt32 size;
    ret = AudioObjectGetPropertyDataSize(
        kAudioObjectSystemObject, &system_devices_addr,
        0, NULL, &size);
    if (ret != noErr) {
        buffer.emitf(EV_LOG_ERROR, "AudioObjectGetPropertyDataSize error 0x%x", ret);
        return;
    }

    UInt32 count = size / sizeof(AudioObjectID);
    AudioObjectID ids[count];
    AudioObjectGetPropertyData(
        kAudioObjectSystemObject, &system_devices_addr,
        0, NULL, &size, (void *) ids);
    if (ret != noErr) {
        buffer.emitf(EV_LOG_ERROR, "AudioObjectGetPropertyData error 0x%x", ret);
        return;
    }

    // Allocate event.
    count = size / sizeof(AudioObjectID);
    auto *ev = buffer.emit(EV_AUDIO_INPUTS_CHANGED, count * sizeof(audio_input_info));
    if (ev == nullptr)
        return;

    auto *infos = (audio_input_info *) ev->data;
    for (UInt32 i = 0; i < count; i++) {
        const auto id = ids[i];
        auto &info = infos[i];

        // Sentinels. If we fail anywhere, or if the device is not an input
        // device, we'll know in the transform function by checking for nulls.
        info.uid = NULL;
        info.name = NULL;

        // Check if it's an input.
        ret = AudioObjectGetPropertyDataSize(id, &device_streams_addr, 0, NULL, &size);
        if (ret != noErr) {
            buffer.emitf(EV_LOG_ERROR, "AudioObjectGetPropertyDataSize error 0x%x", ret);
            continue;
        }
        else if (size == 0) {
            continue;
        }

        // Grab UID.
        CFStringRef uid;
        size = sizeof(uid);
        ret = AudioObjectGetPropertyData(id, &device_uid_addr, 0, NULL, &size, &uid);
        if (ret != noErr) {
            buffer.emitf(EV_LOG_ERROR, "AudioObjectGetPropertyData error: 0x%x\n", ret);
            continue;
        }

        // Grab name.
        CFStringRef name;
        size = sizeof(name);
        ret = AudioObjectGetPropertyData(id, &object_name_addr, 0, NULL, &size, &name);
        if (ret != noErr) {
            CFRelease(uid);
            buffer.emitf(EV_LOG_ERROR, "AudioObjectGetPropertyData error: 0x%x\n", ret);
            continue;
        }

        // Success, transfer ownership to event.
        info.uid = uid;
        info.name = name;
    }
}

static Local<Value> events_transform(
    Isolate *isolate, event &ev, buffer_slicer &slicer)
{
    switch (ev.id) {
        case EV_AUDIO_INPUTS_CHANGED:
            return audio_input_infos_to_js(
                isolate, (audio_input_info *) ev.data,
                ev.size / sizeof(audio_input_info), slicer
            );
        default:
            return Undefined(isolate);
    }
}

static Local<Value> audio_input_infos_to_js(
    Isolate *isolate, audio_input_info *infos,
    uint32_t count, buffer_slicer &slicer)
{
    uint32_t actual_count = count;
    for (uint32_t i = 0; i < count; i++) {
        if (infos[i].uid == NULL)
            actual_count--;
    }

    auto l_device_id_sym = device_id_sym.Get(isolate);
    auto l_name_sym = name_sym.Get(isolate);

    auto arr = Array::New(isolate, actual_count);
    uint32_t arr_idx = 0;
    for (uint32_t i = 0; i < count; i++) {
        auto &info = infos[i];
        if (info.uid == NULL)
            continue;

        auto obj = Object::New(isolate);
        obj->Set(l_device_id_sym, v8_string_from_cf_string(isolate, info.uid));
        obj->Set(l_name_sym, v8_string_from_cf_string(isolate, info.name));
        arr->Set(arr_idx++, obj);

        CFRelease(info.uid);
        CFRelease(info.name);
    }
    return arr;
}

void detect_audio_inputs::init_prototype(Handle<FunctionTemplate> func)
{
    NODE_SET_PROTOTYPE_METHOD(func, "destroy", [](const FunctionCallbackInfo<Value>& args) {
        auto detect = ObjectWrap::Unwrap<detect_audio_inputs>(args.This());
        detect->destroy();
    });
}


}  // namespace p1_mac_plugins
