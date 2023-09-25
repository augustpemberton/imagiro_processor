//
// Created by August Pemberton on 16/11/2022.
//

#pragma once
#include <yaml-cpp/yaml.h>
#include <juce_core/juce_core.h>
#include <shared_processing_code/shared_processing_code.h>

namespace imagiro {
    struct ParameterLoader {
        ParameterLoader(Processor& processor, const juce::String& YAMLString, int maxStreams = 3, int maxMods = 4);
        ParameterLoader(Processor& processor, const juce::File& YAMLFile, int maxStreams = 3, int maxMods = 4);
        std::unique_ptr<imagiro::Parameter> loadParameter(
                const juce::String& uid, YAML::Node paramNode, const juce::String& namePrefix = "", int index = 0);

        imagiro::ParameterConfig loadConfig(juce::String parameterName, juce::String name, YAML::Node p, int index = 0);

        juce::NormalisableRange<float> getScaleRange(float min, float max, juce::String scaleID);
        juce::NormalisableRange<float> getRange(juce::String parameterName, YAML::Node n);

        Processor& processor;

    private:
        void load(const YAML::Node& config, int streams, int mods);
    };
}
