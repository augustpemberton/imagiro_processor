//
// Created by August Pemberton on 08/05/2025.
//

#pragma once
#include "imagiro_processor/src/Processor.h"
#include "Parameters.h"

class GainProcessor : public Processor {
public:
    GainProcessor()
        : Processor(GainProcessorParameters::PARAMETERS_YAML) {
        gainParam = getParameter("gain");
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override {
        Processor::prepareToPlay(sampleRate, samplesPerBlock);
    }

    void process(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) override {
        for (auto c = 0; c < buffer.getNumChannels(); c++) {
            juce::FloatVectorOperations::multiply(
                buffer.getWritePointer(c),
                gainParam->getSmoothedProcessorValueBuffer().getReadPointer(0),
                buffer.getNumSamples()
            );
        }
    }

private:
    Parameter *gainParam;
};
