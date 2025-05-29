//
// Created by August Pemberton on 21/02/2025.
//

#pragma once

#include "../ModGenerator.h"
#include <imagiro_processor/imagiro_processor.h>

#include "Parameters.h"

class LFOGenerator : public ModGenerator {
public:
    LFOGenerator(ModMatrix& m, const std::string& uid, const std::string& name)
        : ModGenerator(LFOGeneratorParameters::PARAMETERS_YAML, m, uid, name)
    {

        depth = getParameter("depth");
        frequency = getParameter("frequency");
        phase = getParameter("phase");

        lfo.setFrequency(0.5f);
        lfo.setShape(LFO::Sine);
    }

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override {
        Processor::prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);
        lfo.setSampleRate(sampleRate);
    }

protected:
    Parameter* depth;
    Parameter* frequency;
    Parameter* phase;

    float advanceGlobalValue(const int numSamples = 1) override {
        lfo.setFrequency(frequency->getProcessorValue());
        lfo.setPhaseOffset(phase->getProcessorValue());
        return lfo.process(numSamples) * depth->getProcessorValue();
    }

    float advanceVoiceValue(size_t voiceIndex, int numSamples = 1) override {
        return 0.f;
    }

private:
    LFO lfo;
};
