//
// Created by August Pemberton on 22/04/2022.
//

#include "Preset.h"
#include "../Processor.h"


namespace imagiro {
    Preset::Preset(bool d)
        : dawState(d),
          data(choc::value::createObject("PresetData")) {
    }

    choc::value::Value Preset::getState() const {
        auto state = choc::value::createObject("Preset");
        state.addMember("name", name);
        state.addMember("description", description);
        state.addMember("data", data);

        auto paramStatesValue = choc::value::createEmptyArray();
        for (const auto& p: paramStates) {
            paramStatesValue.addArrayElement(p.getStateCompressed(isDAWSaveState()));
        }
        state.addMember("paramStates", paramStatesValue);
        state.addMember("modMatrix", serializedModMatrix.getState());

        return state;
    }

    choc::value::Value Preset::getUIState() const {
        auto state = choc::value::createObject("Preset");
        state.addMember("name", name);
        state.addMember("description", description);
        state.addMember("data", data);

        auto paramStatesValue = choc::value::createEmptyArray();
        for (const auto& p: paramStates) {
            paramStatesValue.addArrayElement(p.getState(isDAWSaveState()));
        }

        state.addMember("paramStates", paramStatesValue);
        state.addMember("modMatrix", serializedModMatrix.getState());

        state.addMember("available", available);
        state.addMember("errorString", errorString);
        state.addMember("dawState", dawState);
        return state;
    }

    Preset Preset::fromState(const choc::value::ValueView &state,
                             bool isDAWState,
                             imagiro::Processor* validateProcessor) {
        Preset p (isDAWState);
        if (!state.isObject()) return p;

        p.dawState = state["dawState"].getWithDefault(false);
        p.name = state["name"].getWithDefault("init");
        p.description = state["description"].getWithDefault("");

        for (auto paramState: state["paramStates"]) {
            p.paramStates.push_back(imagiro::Parameter::ParamState::fromState(paramState));
        }

        p.data = state["data"];
        if (state.hasObjectMember("modMatrix")) {
            p.serializedModMatrix = SerializedMatrix::fromState(state["modMatrix"]);
        }


        if (validateProcessor != nullptr) {
            auto error = validateProcessor->isPresetAvailable(p);
            if (!error.has_value()) {
                p.available = true;
                p.errorString = "";
            } else {
                p.available = false;
                p.errorString = error.value();
            }
        }

        return p;
    }

    void Preset::addParamState(imagiro::Parameter::ParamState param) {
        paramStates.push_back(param);
    }

    void Preset::removeParamState(juce::String paramID) {
        paramStates.erase(
                std::remove_if(paramStates.begin(), paramStates.end(), [&](imagiro::Parameter::ParamState& state){
                    return state.uid == paramID;
                }), paramStates.end());
    }

    std::vector<imagiro::Parameter::ParamState> Preset::getParamStates() const {
        return paramStates;
    }
}