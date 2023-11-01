//
// Created by August Pemberton on 22/04/2022.
//

#include "Preset.h"
#include "../config/Resources.h"


Preset::Preset()
    : data(choc::value::createObject("PresetData"))
{

}

choc::value::Value Preset::getState() const {
    auto state = choc::value::createObject("Preset");
    state.addMember("name", name);

    auto paramStatesValue = choc::value::createEmptyArray();
    for (auto p : paramStates) {
        paramStatesValue.addArrayElement(p.getState());
    }

    state.addMember("paramStates", paramStatesValue);
    state.addMember("data", data);

    return state;
}

Preset Preset::fromState(const choc::value::ValueView &state) {
    Preset p;
    p.name = state["name"].getWithDefault("init");

    for (auto paramState : state["paramStates"]) {
        p.paramStates.push_back(imagiro::Parameter::ParamState::fromState(paramState));
    }

    p.data = state["data"];

    return p;
}

void Preset::addParamState(imagiro::Parameter::ParamState param) {
    paramStates.push_back(param);
}

std::vector<imagiro::Parameter::ParamState> Preset::getParamStates() const {
    return paramStates;
}