//
// Created by August Pemberton on 08/05/2025.
//

#pragma once
#include "../../processor/Processor.h"
#include "Parameters.h"

class UtilityProcessor : public Processor {
public:
    UtilityProcessor()
        : Processor(UtilityProcessorParameters::PARAMETERS_YAML,
            ParameterLoader(), getDefaultProperties()) {
        gainParam = getParameter("gain");
        widthParam = getParameter("width");
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
        for (auto s = 0; s < buffer.getNumSamples(); s++) {
            auto gain = gainParam->getSmoothedValue(s);
            auto width = widthParam->getSmoothedValue(s);

            auto l = buffer.getSample(0, s) * gain;
            auto r = buffer.getSample(buffer.getNumChannels() > 1 ? 1 : 0, s) * gain;

            auto mid = l + r;
            auto side = l - r;

            side *= width;

            auto newL = (mid + side) * 0.5f;
            auto newR = (mid - side) * 0.5f;

            buffer.setSample(0, s, newL);
            if (buffer.getNumChannels() > 1) {
                buffer.setSample(1, s, newR);
            }
        }
    }

private:
    Parameter *gainParam;
    Parameter *widthParam;
};
