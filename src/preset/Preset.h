//
// Created by August Pemberton on 22/04/2022.
//

#pragma once
#include "choc/containers/choc_Value.h"
#include "../parameter/Parameter.h"

#define PRESET_EXT ".impreset"

namespace imagiro {
    class Processor;

    class Preset {
    public:
        Preset(bool isDAWSaveState = false);
        virtual ~Preset() = default;
        Preset(const Preset& other) = default;

        virtual choc::value::Value getState() const;
        virtual choc::value::Value getUIState() const;
        static Preset fromState(const choc::value::ValueView& state,
                                bool isDAWState = false,
                                Processor* validateProcessor = nullptr);

        void addParamState(Parameter::ParamState param);
        void removeParamState(juce::String paramID);
        std::vector<Parameter::ParamState> getParamStates() const;

        bool isValid() const { return validPreset; }

        std::string getName() const { return name; }
        std::string getDescription() const { return description; }
        void setName(const std::string& name) {this->name = name;}
        void setDescription(const std::string& description) {this->description = description;}

        choc::value::Value& getData() { return data; }

        bool isDAWSaveState() const { return dawState; }
        bool isAvailable() const { return available; }

        void setModMatrix(SerializedMatrix m) { serializedModMatrix = m; }
        const SerializedMatrix& getModMatrix() { return serializedModMatrix; }

    protected:
        bool dawState {false};
        std::string name;
        std::string description;
        choc::value::Value data;

        std::vector<Parameter::ParamState> paramStates;
        SerializedMatrix serializedModMatrix;

        bool validPreset;
        bool available {true};
        std::string errorString;

        JUCE_LEAK_DETECTOR (Preset)
    };
}