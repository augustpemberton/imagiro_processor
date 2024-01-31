//
// Created by August Pemberton on 22/04/2022.
//

#include "Preset.h"
#include "../config/Resources.h"


Preset::Preset(bool d)
    : dawState(d),
      data(choc::value::createObject("PresetData")) {
}

choc::value::Value Preset::getState() const {
    auto state = choc::value::createObject("Preset");
    state.addMember("name", name);
    state.addMember("dawState", dawState);

    auto paramStatesValue = choc::value::createEmptyArray();
    for (const auto& p: paramStates) {
        paramStatesValue.addArrayElement(p.getState());
    }

    state.addMember("paramStates", paramStatesValue);
    state.addMember("data", data);
    state.addMember("available", available);

    return state;
}

Preset Preset::fromState(const choc::value::ValueView &state, imagiro::Processor* validateProcessor) {
    Preset p;
    if (!state.isObject()) return p;

    p.dawState = state["dawState"].getWithDefault(false);
    p.name = state["name"].getWithDefault("init");

    for (auto paramState: state["paramStates"]) {
        p.paramStates.push_back(imagiro::Parameter::ParamState::fromState(paramState));
    }

    p.data = state["data"];
    if (validateProcessor != nullptr) p.available = validateProcessor->isPresetAvailable(p);

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
