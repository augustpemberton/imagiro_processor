//
// Created by August Pemberton on 14/07/2025.
//

#pragma once
#include "GrainSettings.h"
#include <nlohmann/json.hpp>
#include "juce_core/juce_core.h"
#include "../valuedata/Serialize.h"

using json = nlohmann::json;

// LoopSettings serializer
template<>
struct Serializer<LoopSettings> {
    static json serialize(const LoopSettings& settings) {
        json val = json::object();
        val["loopStart"] = settings.loopStart;
        val["loopLength"] = settings.loopLength;
        val["loopCrossfade"] = settings.loopCrossfade;
        val["loopActive"] = settings.loopActive;
        return val;
    }

    static LoopSettings load(const json& state) {
        LoopSettings settings;
        try {
            settings.loopStart = state.value("loopStart", 0.0f);
            settings.loopLength = state.value("loopLength", 0.0f);
            settings.loopCrossfade = state.value("loopCrossfade", 0.0f);
            settings.loopActive = state.value("loopActive", false);
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
    static json serialize(const GrainSettings& settings) {
        json val = json::object();
        val["duration"] = settings.duration;
        val["position"] = settings.position;
        val["loopSettings"] = Serializer<LoopSettings>::serialize(settings.loopSettings);
        val["gain"] = settings.gain;
        val["pitch"] = settings.pitch;
        val["pan"] = settings.pan;
        val["spread"] = settings.spread;
        val["shape"] = settings.shape;
        val["skew"] = settings.skew;
        val["reverse"] = settings.reverse;
        return val;
    }

    static GrainSettings load(const json& state) {
        GrainSettings settings;
        try {
            settings.duration = state.value("duration", 0.5f);
            settings.position = state.value("position", 0.0f);
            settings.loopSettings = Serializer<LoopSettings>::load(state["loopSettings"]);
            settings.gain = state.value("gain", 1.0f);
            settings.pitch = state.value("pitch", 0.0f);
            settings.pan = state.value("pan", 0.5f);
            settings.spread = state.value("spread", 0.0f);
            settings.shape = state.value("shape", 0.5f);
            settings.skew = state.value("skew", 0.5f);
            settings.reverse = state.value("reverse", false);
        } catch (...) {
            DBG("Unable to parse GrainSettings!");
            jassertfalse;
        }
        return settings;
    }
};
