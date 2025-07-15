//
// Created by August Pemberton on 14/07/2025.
//

#pragma once
#include "GrainSettings.h"
#include <choc/containers/choc_Value.h>
#include "juce_core/juce_core.h"
#include "../valuedata/Serialize.h"

// LoopSettings serializer
template<>
struct Serializer<LoopSettings> {
    static choc::value::Value serialize(const LoopSettings& settings) {
        auto val = choc::value::createObject("LoopSettings");
        val.setMember("loopStart", choc::value::Value{settings.loopStart});
        val.setMember("loopLength", choc::value::Value{settings.loopLength});
        val.setMember("loopCrossfade", choc::value::Value{settings.loopCrossfade});
        val.setMember("loopActive", choc::value::Value{settings.loopActive});
        return val;
    }

    static LoopSettings load(const choc::value::ValueView& state) {
        LoopSettings settings;
        try {
            settings.loopStart = state["loopStart"].getWithDefault(0.0f);
            settings.loopLength = state["loopLength"].getWithDefault(0.0f);
            settings.loopCrossfade = state["loopCrossfade"].getWithDefault(0.0f);
            settings.loopActive = state["loopActive"].getWithDefault(false);
        } catch (...) {
            DBG("Unable to parse LoopSettings!");
            jassertfalse;
        }
        return settings;
    }
};

// GrainSettings serializer
template<>
struct Serializer<GrainSettings> {
    static choc::value::Value serialize(const GrainSettings& settings) {
        auto val = choc::value::createObject("GrainSettings");
        val.setMember("duration", choc::value::Value{settings.duration});
        val.setMember("position", choc::value::Value{settings.position});
        val.setMember("loopSettings", Serializer<LoopSettings>::serialize(settings.loopSettings));
        val.setMember("gain", choc::value::Value{settings.gain});
        val.setMember("pitch", choc::value::Value{settings.pitch});
        val.setMember("pan", choc::value::Value{settings.pan});
        val.setMember("spread", choc::value::Value{settings.spread});
        val.setMember("shape", choc::value::Value{settings.shape});
        val.setMember("skew", choc::value::Value{settings.skew});
        val.setMember("reverse", choc::value::Value{settings.reverse});
        return val;
    }

    static GrainSettings load(const choc::value::ValueView& state) {
        GrainSettings settings;
        try {
            settings.duration = state["duration"].getWithDefault(0.5f);
            settings.position = state["position"].getWithDefault(0.0f);
            settings.loopSettings = Serializer<LoopSettings>::load(state["loopSettings"]);
            settings.gain = state["gain"].getWithDefault(1.0f);
            settings.pitch = state["pitch"].getWithDefault(0.0f);
            settings.pan = state["pan"].getWithDefault(0.5f);
            settings.spread = state["spread"].getWithDefault(0.0f);
            settings.shape = state["shape"].getWithDefault(0.5f);
            settings.skew = state["skew"].getWithDefault(0.5f);
            settings.reverse = state["reverse"].getWithDefault(false);
        } catch (...) {
            DBG("Unable to parse GrainSettings!");
            jassertfalse;
        }
        return settings;
    }
};