//
// Created by August Pemberton on 16/11/2022.
//

#pragma once
#include <yaml-cpp/yaml.h>
#include <juce_core/juce_core.h>

namespace imagiro {
    class Parameter;
    class Processor;
    struct ParameterConfig;
    struct ParameterLoader {
        virtual ~ParameterLoader() = default;

        /*
         * Create parameters from a YAML string.
         * a processor is needed for tempo-sync parameters
         */
        std::vector<std::unique_ptr<Parameter>> loadParameters(const juce::String& YAMLString, Processor& processor) const;

    protected:
        virtual std::vector<std::unique_ptr<Parameter>> loadParametersFromNode(const juce::String& uid, YAML::Node p, Processor& processor) const ;
        virtual ParameterConfig loadConfig(const juce::String& name, YAML::Node p, Processor& processor) const;

        static juce::String getString(const YAML::Node& n, const juce::String& key, juce::String defaultValue = "");
        static float getFloat(const YAML::Node& n, const juce::String& key, float defaultValue = 0);
        static bool getBool(const YAML::Node& n, const juce::String& key, bool defaultValue = false);
        static juce::NormalisableRange<float> getRange(const YAML::Node& n);
    };
}
