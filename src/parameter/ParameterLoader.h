//
// Created by August Pemberton on 16/11/2022.
//

#pragma once
#include <yaml-cpp/yaml.h>
#include <juce_core/juce_core.h>

namespace imagiro {
    struct ParameterLoader {
        ParameterLoader(Processor& processor, const juce::String& YAMLString);

        imagiro::ParameterConfig loadConfig(juce::String parameterName, juce::String name, YAML::Node p, int index = 0);

    private:
        std::unique_ptr<imagiro::Parameter> loadParameter(
                const juce::String& uid, YAML::Node paramNode, const juce::String& namePrefix = "", int index = 0);
        juce::NormalisableRange<float> getRange(juce::String parameterName, YAML::Node n);
        void load(const YAML::Node& config);

        Processor& processor;
    };
}
