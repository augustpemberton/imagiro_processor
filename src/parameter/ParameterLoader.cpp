
//
// Created by August Pemberton on 16/11/2022.
//

#include "ParameterLoader.h"

#include <utility>

namespace imagiro {
    std::vector<std::unique_ptr<Parameter>> ParameterLoader::loadParameters(const juce::String& YAMLString, Processor& processor) const {
        std::vector<std::unique_ptr<Parameter>> parameters;

        auto rootNode = YAML::Load(YAMLString.toStdString());
        for (const auto& kv : rootNode) {
            juce::String uid (kv.first.as<std::string>());

            auto params = loadParametersFromNode(uid, kv.second, processor);
            for (auto& param : params) {
                parameters.push_back(std::move(param));
            }
        }

        return parameters;
    }

    std::vector<std::unique_ptr<Parameter>> ParameterLoader::loadParametersFromNode(const juce::String& uid, YAML::Node p, Processor& processor) const {
        auto name = getString(p, "name");
        auto isInternal = getBool(p, "internal", false);
        auto isMeta = getBool(p, "meta", false);
        auto isAutomatable = getBool(p, "automatable", true);
        auto defaultJitter = getFloat(p, "jitter", 0.f);
        std::vector<ParameterConfig> configs;

        // Multi-configs
        if (p["configs"]) {
            for (auto config : p["configs"])
                configs.push_back(loadConfig(config.first.as<std::string>(), config.second, processor));
        } else {
            // Single config
            configs.push_back(loadConfig("default", p, processor));
        }

        std::vector<std::unique_ptr<Parameter>> params;
        auto parameter = std::make_unique<Parameter>(uid.toStdString(), name.toStdString(), configs,
                                                 isInternal, isMeta, isAutomatable, defaultJitter);
        params.push_back(std::move(parameter));
        return params;
    }

    ParameterConfig ParameterLoader::loadConfig(const juce::String& configName, YAML::Node p, Processor& processor) const {
        auto type = getString(p, "type", "number");
        auto range = getRange(p["range"]);
        auto reverse = getBool(p, "reverse", false);

        auto numSteps = range.getRange().getLength() / (float)range.interval;
        auto discrete = numSteps < 100;

        float defaultVal = 0;
        defaultVal = getFloat(p, "default", 0);

        ParameterConfig config {range, discrete, defaultVal};
        config.name = configName.toStdString();
        config.reverse = reverse;
        config.textFunction = [&] (float val) -> DisplayValue { return {juce::String(val)}; };
        config.valueFunction = [&] (const juce::String& s) -> float { return s.getFloatValue(); };

        auto syncType = getString(p, "sync", "normal");

//        if (type == "number") {
//           // nothing to do
        if (type == "percent") {
            config.textFunction = DisplayFunctions::percentDisplay;
            config.valueFunction = DisplayFunctions::percentInput;
        } else if (type == "db") {
            config.textFunction = DisplayFunctions::dbDisplay;
            config.valueFunction = DisplayFunctions::dbInput;
            config.processorConversionFunction = [](float val) {
                if (val <= -60) return 0.f;
                return juce::Decibels::decibelsToGain(val);
            };
        } else if (type == "time") {
            config.textFunction = DisplayFunctions::timeDisplay;
            config.valueFunction = DisplayFunctions::timeInput;
        } else if (type == "samples") {
            config.textFunction = [&] (float samples) -> DisplayValue {
                return DisplayFunctions::timeDisplay(samples/(float)processor.getLastSampleRate());
            };
            config.valueFunction = [&] (const juce::String& s) -> float {
                return DisplayFunctions::timeInput(s) * (float)processor.getLastSampleRate();
            };
        } else if (type == "freq") {
            config.textFunction = DisplayFunctions::freqDisplay;
            config.valueFunction = DisplayFunctions::freqInput;
        } else if (type == "degrees") {
            config.textFunction = DisplayFunctions::degreeDisplay;
            config.valueFunction = DisplayFunctions::degreeInput;
        } else if (type == "toggle") {
            config.range = {0, 1, 1};
            config.textFunction = [](float choice) -> DisplayValue {
                return {choice != 0.f ? "on" : "off"};
            };
        } else if (type == "semitone") {
            config.textFunction = DisplayFunctions::centDisplay;
            config.valueFunction = DisplayFunctions::centInput;
        } else if (type == "cent") {
            config.textFunction = DisplayFunctions::centDisplay;
            config.valueFunction = DisplayFunctions::centInput;
        } else if (type == "sync") {
            config.textFunction = DisplayFunctions::syncDisplay;
            config.valueFunction = DisplayFunctions::syncInput;
            config.processorValueChangesWithBPM = true;
            config.processorConversionFunction = [&, syncType](float proportion) {
                auto v = (processor.getSyncTimeSeconds(proportion));
                return (syncType == "inverse") ? 1.f / v : v;
            };
        } else if (type == "sync-dotted") {
            config.textFunction = [](float t) -> DisplayValue {
                return {DisplayFunctions::syncDisplay(t).value, "dotted"};
            };
            config.valueFunction = [](const juce::String& frac) {
                return DisplayFunctions::syncInput(frac.replace("d", "", true));
            };

            config.processorValueChangesWithBPM = true;
            config.processorConversionFunction = [&, syncType](float proportion) {
                auto v = (processor.getSyncTimeSeconds(proportion) * (3.f/2.f));
                return (syncType == "inverse") ? 1.f / v : v;
            };
        } else if (type == "sync-triplet") {
            config.textFunction = [](float t) -> DisplayValue {
                return {DisplayFunctions::syncDisplay(t).value, "triplet"};
            };
            config.valueFunction = [](const juce::String& frac) {
                return DisplayFunctions::syncInput(frac.replace("t", "", true));
            };

            config.processorValueChangesWithBPM = true;
            config.processorConversionFunction = [&, syncType](float proportion) {
                auto v = (processor.getSyncTimeSeconds(proportion) * (2.f/3.f));
                return (syncType == "inverse") ? 1.f / v : v;
            };
        } else if (type == "choice") {
            auto choices = p["choices"].as<std::vector<std::string>>();
            config.textFunction = [choices](float choice)->DisplayValue {
                return {choices[(unsigned int)std::round(choice)]};
            };
            config.valueFunction = [choices](const juce::String& choice) {
                return std::find(choices.begin(), choices.end(), choice) - choices.begin();
            };
            config.range = {0, (float)choices.size()-1, 1};
            config.choices = choices;
        } else if (type == "ratio") {
            auto ratioParams = p["ratio"];
            juce::String ratioParamName = ratioParams["name"].as<std::string>();
            Parameter* ratioParam;

            ratioParam = processor.getParameter(ratioParamName);

            juce::String ratioParamDisplayName = getString(ratioParams, "display",
                                                           ratioParam->getName(100));

            config.textFunction = [&, ratioParamDisplayName](float v) -> DisplayValue {
                if (v > 1) return {juce::String(v, 2)+":1", "-> " + ratioParamDisplayName};
                else return {"1:"+juce::String(1/v, 2), "-> " + ratioParamDisplayName};
            };

            config.valueFunction = [&, ratioParamDisplayName](juce::String v) -> float {
                v = v.upToFirstOccurrenceOf("->", false, true);
                juce::StringArray ratio;
                ratio.addTokens(v, ":", "");
                if (ratio.size() != 2) return v.getFloatValue();

                return ratio[0].getFloatValue() / ratio[1].getFloatValue();
            };

            config.processorConversionFunction = [&, ratioParam](float v) {
                return ratioParam->getValue() * v;
            };
        }

        return config;
    }


    juce::String ParameterLoader::getString(const YAML::Node &n, const juce::String &key, juce::String defaultValue) {
        if (!n) return defaultValue;
        auto k = key.toStdString();
        return (n[k] ? n[k].as<std::string>() : defaultValue);
    }

    float ParameterLoader::getFloat(const YAML::Node &n, const juce::String &key, float defaultValue) {
        if (!n) return defaultValue;
        auto k = key.toStdString();
        return (n[k] ? n[k].as<float>() : defaultValue);
    }

    bool ParameterLoader::getBool(const YAML::Node &n, const juce::String &key, bool defaultValue) {
        if (!n) return defaultValue;
        auto k = key.toStdString();
        return (n[k] ? n[k].as<bool>() : defaultValue);
    }

    juce::NormalisableRange<float> ParameterLoader::getRange(const YAML::Node& n) {
        auto type = getString(n, "type", "normal");
        auto step = getFloat(n, "step", 0);

        if (type == "freq") {
            auto min = getFloat(n, "min", 20);
            auto max = getFloat(n, "max", 20000);
            return getNormalisableRangeExp(min, max, step);
        }

        auto min = getFloat(n, "min", 0);
        auto max = getFloat(n, "max", 1);
        auto skew = getFloat(n, "skew", 1);
        auto inverse = getBool(n, "inverse", false);
        auto symmetricSkew = getBool(n, "symmetricSkew", false);

        if (type == "exp")
            return getNormalisableRangeExp(min, max, step);

        if (type == "sync")
            return getTempoSyncRange((int)min, (int)max, inverse);

        return {min, max, step, skew, symmetricSkew};
    }


}
