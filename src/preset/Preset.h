//
// Created by August Pemberton on 22/04/2022.
//

#pragma once
#include <juce_data_structures/juce_data_structures.h>
#include <utility>
#include "choc/containers/choc_Value.h"

#define PRESET_EXT ".impreset"

class Preset {
public:
    Preset();

    virtual choc::value::Value getState() const;
    static Preset fromState(const choc::value::ValueView& state);

    void addParamState(imagiro::Parameter::ParamState param);
    std::vector<imagiro::Parameter::ParamState> getParamStates() const;

    bool isValid() const { return validPreset; }

    std::string getName() const { return name; }
    void setName(const std::string& name) {this->name = name;}

    choc::value::Value& getData() { return data; }

protected:
    std::string name;
    choc::value::Value data;

    std::vector<imagiro::Parameter::ParamState> paramStates;

    bool validPreset;

    JUCE_LEAK_DETECTOR (Preset)
};

