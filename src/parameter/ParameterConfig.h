//
// Created by August Pemberton on 16/11/2022.
//

#pragma once
#include "DisplayValue.h"

namespace imagiro {
    class Parameter;
    struct ParameterConfig {
        juce::NormalisableRange<float> range {0, 1};
        bool discrete {false};
        float defaultValue {0};
        std::function<float(float, const Parameter*)> processorConversionFunction {nullptr};
        std::function<DisplayValue(float, const Parameter*)> textFunction {nullptr};
        std::function<float(const juce::String&, const Parameter*)> valueFunction {nullptr};
        std::string name {"default"};
        bool reverse {false};
        std::vector<std::string> choices = {};
        bool processorValueChangesWithBPM = false;
    };
}
