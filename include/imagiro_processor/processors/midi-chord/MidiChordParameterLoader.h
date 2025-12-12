//
// Created by August Pemberton on 22/01/2025.
//

#pragma once
#include <imagiro_processor/imagiro_processor.h>

class MidiChordParameterLoader : public ParameterLoader {
public:
    MidiChordParameterLoader(int numShifts) : numShifts(numShifts) { }

protected:
    std::vector<std::unique_ptr<Parameter>> loadMultiParam(const juce::String &uid, YAML::Node paramNode, Processor& p, int numCopies) const {
        std::vector<std::unique_ptr<Parameter>> parameters;

        const auto multiParamUID = "shift";
        const auto multiParamNode = paramNode;

        const auto baseParams = ParameterLoader::loadParametersFromNode(multiParamUID, multiParamNode, p);

        for (auto& baseParam : baseParams) {
            auto multiParams = makeMultiParameters(*baseParam, numCopies);
            for (auto& multiParam : multiParams) {
                parameters.push_back(std::move(multiParam));
            }
        }

        return parameters;
    }

    virtual std::vector<std::unique_ptr<imagiro::Parameter>> loadParametersFromNode(const juce::String &uid,
                                                                            YAML::Node paramNode, Processor& p) const override
    {
        if (uid == "shift") return loadMultiParam(uid, paramNode, p, numShifts);
        return ParameterLoader::loadParametersFromNode(uid, paramNode, p);

    }

    /*
     * Create multiple parameters based on one template parameter.
     */
    std::vector<std::unique_ptr<imagiro::Parameter>> makeMultiParameters (const Parameter& templateParam, int numCopies) const {
        std::vector<std::unique_ptr<Parameter>> parameters;

        for (auto i=0; i < numCopies; i++) {
            auto streamParamUID = templateParam.getParameterID() + "-" + juce::String(i);
            auto streamName = templateParam.getName(100) + " " + juce::String(i+1);

            parameters.push_back(std::make_unique<Parameter>(
                    streamParamUID.toStdString(),
                    streamName.toStdString(),
                    templateParam.getAllConfigs(),
                    templateParam.isMeta(),
                    templateParam.isInternal(),
                    templateParam.isAutomatable(),
                    templateParam.getJitterAmount()
            ));
        }

        return parameters;
    }

private:
    int numShifts;
};
