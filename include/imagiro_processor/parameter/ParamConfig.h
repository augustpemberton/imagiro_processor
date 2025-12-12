//
// Created by August Pemberton on 11/12/2025.
//

#pragma once
#include <functional>
#include <string>

#include "ParamFormat.h"
#include "ParamRange.h"

namespace imagiro {
    struct ParamConfig {
        std::string uid;
        std::string name;
        ParamRange range;
        ParamFormat format{ParamFormat::number()};
        float (*toProcessor)(float userValue, double bpm, double sampleRate){nullptr};

        float defaultValue{0.f};
    };

    // --- Factory helpers ---

    inline ParamConfig makeGainParam(std::string uid, std::string name,
                                     float minDb = -60.f, float maxDb = 12.f, float defaultDb = 0.f) {
        return {
            .uid = std::move(uid),
            .name = std::move(name),
            .range = ParamRange::linear(minDb, maxDb),
            .format = ParamFormat::decibels(),
            .toProcessor = +[](float db, double, double) {
                return juce::Decibels::decibelsToGain(db);
            },
            .defaultValue = defaultDb
        };
    }

    inline ParamConfig makeFrequencyParam(std::string uid, std::string name,
                                          float minHz = 20.f, float maxHz = 20000.f, float defaultHz = 1000.f) {
        return {
            .uid = std::move(uid),
            .name = std::move(name),
            .range = ParamRange::frequency(minHz, maxHz),
            .format = ParamFormat::frequency(),
            .defaultValue = defaultHz
        };
    }

    inline ParamConfig makeTimeParam(std::string uid, std::string name,
                                     float minSec = 0.001f, float maxSec = 10.f, float defaultSec = 0.1f) {
        return {
            .uid = std::move(uid),
            .name = std::move(name),
            .range = ParamRange::logarithmic(minSec, maxSec),
            .format = ParamFormat::time(),
            .defaultValue = defaultSec
        };
    }

    inline ParamConfig makePercentParam(std::string uid, std::string name, float defaultVal = 0.5f) {
        return {
            .uid = std::move(uid),
            .name = std::move(name),
            .range = ParamRange::linear(0.f, 1.f),
            .format = ParamFormat::percent(),
            .defaultValue = defaultVal
        };
    }

    inline ParamConfig makeToggleParam(std::string uid, std::string name, bool defaultVal = false) {
        return {
            .uid = std::move(uid),
            .name = std::move(name),
            .range = ParamRange::toggle(),
            .format = ParamFormat::toggle(),
            .defaultValue = defaultVal ? 1.f : 0.f
        };
    }

    inline ParamConfig makeChoiceParam(std::string uid, std::string name,
                                       std::vector<std::string> choices, int defaultIndex = 0) {
        return {
            .uid = std::move(uid),
            .name = std::move(name),
            .range = ParamRange::choice(static_cast<int>(choices.size())),
            .format = ParamFormat::choice(choices),
            .defaultValue = static_cast<float>(defaultIndex)
        };
    }

    inline ParamConfig makeLinearParam(std::string uid, std::string name,
                                       float min, float max, float defaultVal,
                                       const std::string &suffix = "", int decimals = 2) {
        return {
            .uid = std::move(uid),
            .name = std::move(name),
            .range = ParamRange::linear(min, max),
            .format = ParamFormat::withSuffix(suffix, decimals),
            .defaultValue = defaultVal
        };
    }
}