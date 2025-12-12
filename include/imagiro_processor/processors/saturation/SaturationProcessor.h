//
// Created by August Pemberton on 08/05/2025.
//

#pragma once
#include "../../processor/Processor.h"
#include "Parameters.h"

using namespace imagiro;
class SaturationProcessor : public Processor {
public:
    SaturationProcessor()
        : Processor(SaturationProcessorParameters::PARAMETERS_YAML,
            ParameterLoader(), getDefaultProperties()) {
        driveParam = getParameter("drive");
    }

    BusesProperties getDefaultProperties() {
        return BusesProperties()
            .withInput("Input", juce::AudioChannelSet::stereo(), true)
            .withOutput("Output", juce::AudioChannelSet::stereo(), true);
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override {
        Processor::prepareToPlay(sampleRate, samplesPerBlock);
    }

    void process(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) override {
        for (auto c = 0; c < buffer.getNumChannels(); c++) {
            for (auto s = 0; s < buffer.getNumSamples(); s++) {
                auto drive = driveParam->getSmoothedValue(s);
                auto v = buffer.getSample(c, s);

                v *= drive;
                v = std::tanh(v);
                v /= drive;

                buffer.setSample(c, s, v);
            }
        }
    }

private:
    Parameter *driveParam;
};
