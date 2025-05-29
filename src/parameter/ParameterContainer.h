//
// Created by August Pemberton on 28/05/2025.
//

#pragma once
#include "imagiro_processor/imagiro_processor.h"


class ParameterContainer {
public:
    explicit ParameterContainer(const juce::String &parametersYAMLString,
                                Processor& processorForDisplayFunctions,
                                 const ParameterLoader &loader = ParameterLoader()) {
        auto parameters = loader.loadParameters(parametersYAMLString, processorForDisplayFunctions);
        for (auto &param: parameters) this->addParam(std::move(param));
    }

    virtual ~ParameterContainer() = default;

    Parameter* getParameter (const juce::String& uid) { return parameterMap.contains(uid) ? parameterMap[uid] : nullptr; }
    const std::vector<std::unique_ptr<Parameter>>& getParameters() { return parameters; }

private:
    std::map<juce::String, Parameter *> parameterMap;
    std::vector<std::unique_ptr<Parameter>> parameters;

    void addParam(std::unique_ptr<Parameter> param) {
        parameterMap.insert({param->getUID(), param.get()});
        parameters.push_back(std::move(param));
    }
};
