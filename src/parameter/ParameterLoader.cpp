//
// Created by August Pemberton on 16/11/2022.
//

#include "ParameterLoader.h"

namespace imagiro {
    ParameterLoader::ParameterLoader(Processor& processor, const juce::String& yamlString)
            : processor(processor) {
        auto params = YAML::Load(yamlString.toStdString());
        load(params);
    }

    void ParameterLoader::load(const YAML::Node& config) {
        for (const auto& kv : config) {
            juce::String uid (kv.first.as<std::string>());
            processor.addParam(loadParameter(uid, kv.second));
        }
    }

    static juce::String str (const YAML::Node& n) {
        return {n.as<std::string>()};
    }

    static float flt (const YAML::Node& n) {
        return n.as<float>();
    }

    static juce::String getString(const YAML::Node& n, const juce::String& key, juce::String defaultValue) {
        if (!n) return defaultValue;
        auto k = key.toStdString();
        return (n[k] ? str(n[k]) : defaultValue);
    }

    static float getFloat(const YAML::Node& n, const juce::String& key, float defaultValue) {
        if (!n) return defaultValue;
        auto k = key.toStdString();
        return (n[k] ? n[k].as<float>() : defaultValue);
    }

    static bool getBool(const YAML::Node& n, const juce::String& key, bool defaultValue) {
        if (!n) return defaultValue;
        auto k = key.toStdString();
        return (n[k] ? n[k].as<bool>() : defaultValue);
    }

    juce::NormalisableRange<float> ParameterLoader::getRange(juce::String parameterID, YAML::Node n) {
        auto type = getString(n, "type", "normal");

        if (type == "freq") {
            return getNormalisableRangeExp(20, 20000);
        }

        auto min = getFloat(n, "min", 0);
        auto max = getFloat(n, "max", 1);
        auto step = getFloat(n, "step", 0);
        auto skew = getFloat(n, "skew", 1);
        auto inverse = getBool(n, "inverse", false);
        auto symmetricSkew = getBool(n, "symmetricSkew", false);

        if (type == "exp")
            return getNormalisableRangeExp(min, max, step);

        if (type == "sync")
            return getTempoSyncRange(min, max, inverse);

        return {min, max, step, skew, symmetricSkew};
    }


    ParameterConfig ParameterLoader::loadConfig(juce::String parameterID, juce::String configName, YAML::Node p, int index) {
        auto type = getString(p, "type", "number");
        auto range = getRange(parameterID, p["range"]);
        auto reverse = getBool(p, "reverse", false);

        auto numSteps = range.getRange().getLength() / (float)range.interval;
        auto discrete = numSteps < 100;

        float defaultVal = 0;
        if (p["default"].Type() == YAML::NodeType::Scalar)
            defaultVal = getFloat(p, "default", 0);
        else if (p["default"].Type() == YAML::NodeType::Sequence) {
            auto size = p["default"].size();
            defaultVal = p["default"][std::min(index, (int)size - 1)].as<float>();
        }

        ParameterConfig config {range, discrete, defaultVal};
        config.name = configName.toStdString();
        config.reverse = reverse;
        config.textFunction = [&] (const Parameter& p, float val) -> DisplayValue { return {juce::String(val)}; };
        config.valueFunction = [&] (const Parameter& p, const juce::String& s) -> float { return s.getFloatValue(); };

        auto syncType = getString(p, "sync", "normal");

        if (type == "number") {
            // nothing to do
        } else if (type == "percent") {
            config.textFunction = DisplayFunctions::percentDisplay;
            config.valueFunction = DisplayFunctions::percentInput;
        } else if (type == "db") {
            config.textFunction = DisplayFunctions::dbDisplay;
            config.valueFunction = DisplayFunctions::dbInput;
            config.processorConversionFunction = [](float val) {
                return juce::Decibels::decibelsToGain(val);
            };
        } else if (type == "time") {
            config.textFunction = DisplayFunctions::timeDisplay;
            config.valueFunction = DisplayFunctions::timeInput;
        } else if (type == "samples") {
            config.textFunction = [&] (const Parameter& p, float samples) -> DisplayValue {
                return DisplayFunctions::timeDisplay(p, samples/processor.getLastSampleRate());
            };
            config.valueFunction = [&] (const Parameter& p, juce::String s) -> float {
                return DisplayFunctions::timeInput(p, s) * processor.getLastSampleRate();
            };
        } else if (type == "freq") {
            config.textFunction = DisplayFunctions::freqDisplay;
            config.valueFunction = DisplayFunctions::freqInput;
        } else if (type == "degrees") {
            config.textFunction = DisplayFunctions::degreeDisplay;
            config.valueFunction = DisplayFunctions::degreeInput;
        } else if (type == "toggle") {
            config.range = {0, 1, 1};
            config.textFunction = [](const Parameter& p, float choice)->DisplayValue {
                return {p.getBoolValue() ? "on" : "off"};
            };
        } else if (type == "semitone") {
            config.textFunction = DisplayFunctions::semitoneDisplay;
            config.valueFunction = DisplayFunctions::semitoneInput;
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
            config.textFunction = [](const Parameter& p, float t) -> DisplayValue {
                return {DisplayFunctions::syncDisplay(p, t).value, "d"};
            };
            config.valueFunction = [](const Parameter& p, juce::String frac) {
                return DisplayFunctions::syncInput(p,
                           frac.replace("d", "", true));
            };

            config.processorValueChangesWithBPM = true;
            config.processorConversionFunction = [&, syncType](float proportion) {
                auto v = (processor.getSyncTimeSeconds(proportion) * (3.f/2.f));
                return (syncType == "inverse") ? 1.f / v : v;
            };
        } else if (type == "sync-triplet") {
            config.textFunction = [](const Parameter& p, float t) -> DisplayValue {
                return {DisplayFunctions::syncDisplay(p, t).value, "t"};
            };
            config.valueFunction = [](const Parameter& p, juce::String frac) {
                return DisplayFunctions::syncInput(p,
                               frac.replace("t", "", true));
            };

            config.processorValueChangesWithBPM = true;
            config.processorConversionFunction = [&, syncType](float proportion) {
                auto v = (processor.getSyncTimeSeconds(proportion) * (2.f/3.f));
                return (syncType == "inverse") ? 1.f / v : v;
            };
        } else if (type == "choice") {
            auto choices = p["choices"].as<std::vector<std::string>>();
            config.textFunction = [choices](const Parameter& /*p*/, float choice)->DisplayValue {
                return {choices[(unsigned int)std::round(choice)]};
            };
            config.valueFunction = [choices](const Parameter& p, juce::String choice) {
                return std::find(choices.begin(), choices.end(), choice) - choices.begin();
            };
            config.range = {0, (float)choices.size()-1, 1};
            config.choices = choices;
        } else if (type == "ratio") {
            auto ratioParams = p["ratio"];
            juce::String ratioParamName = ratioParams["name"].as<std::string>();
            Parameter* ratioParam;

            if (ratioParams["type"].as<std::string>() == "stream")
                ratioParam = processor.getParameter(ratioParamName + "-" + juce::String(index));
            else
                ratioParam = processor.getParameter(ratioParamName);

            juce::String ratioParamDisplayName = getString(ratioParams, "display",
                                                           ratioParam->getName(100));

            config.textFunction = [&, ratioParamDisplayName](const Parameter& p, float v) -> DisplayValue {
                if (v > 1) return {juce::String(v, 2)+":1", "-> " + ratioParamDisplayName};
                else return {"1:"+juce::String(1/v, 2), "-> " + ratioParamDisplayName};
            };

            config.valueFunction = [&, ratioParamDisplayName](const Parameter& p, juce::String v) -> float {
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

    std::unique_ptr<Parameter> ParameterLoader::loadParameter(const juce::String& uid, YAML::Node p,
                                                              const juce::String& namePrefix, int index) {
        auto name = namePrefix + str(p["name"]);
        auto isInternal = getBool(p, "internal", false);
        auto isMeta = getBool(p, "meta", false);
        auto isAutomatable = getBool(p, "automatable", true);
        std::vector<ParameterConfig> configs;

        // Multi-configs
        if (p["configs"]) {
            for (auto config : p["configs"])
                configs.push_back(loadConfig(uid, config.first.as<std::string>(), config.second, index));
        } else {
            // Single config
            configs.push_back(loadConfig(uid, "default", p, index));
        }

        return std::make_unique<Parameter>(uid.toStdString(), name.toStdString(), configs,
                                           isMeta, isInternal, isAutomatable);
    }
}
