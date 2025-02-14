//
// Created by August Pemberton on 16/11/2022.
//

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>


namespace imagiro {
    struct DisplayValue;
    struct ParameterConfig {
        juce::NormalisableRange<float> range {0, 1};
        bool discrete {false};
        float defaultValue {0};
        std::function<float(float)> processorConversionFunction {nullptr};
        std::function<DisplayValue(float)> textFunction {nullptr};
        std::function<float(const juce::String&)> valueFunction {nullptr};
        std::string name {"default"};
        bool reverse {false};
        std::vector<std::string> choices = {};
        bool processorValueChangesWithBPM = false;
    };
}
