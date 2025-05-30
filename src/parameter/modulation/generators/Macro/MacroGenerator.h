//
// Created by August Pemberton on 21/02/2025.
//

#pragma once

#include "../ModGenerator.h"
#include <imagiro_processor/imagiro_processor.h>

#include "Parameters.h"

class MacroGenerator : public ModGenerator {
public:
    MacroGenerator(ModMatrix &m, const std::string &uid, const std::string &name)
        : ModGenerator(MacroGeneratorParameters::PARAMETERS_YAML, m, uid, name)
    {
        //
    }

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override {
        Processor::prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);
        //
    }

protected:
    Parameter *value { getParameter("value") };

    float advanceGlobalValue(const int numSamples = 1) override {
        return value->getProcessorValue();
    }

    float advanceVoiceValue(size_t voiceIndex, int numSamples = 1) override {
        return 0.f;
    }
};
