//
// Created by August Pemberton on 08/05/2025.
//

#pragma once
#include "imagiro_processor/src/Processor.h"
#include "Parameters.h"
#include "imagiro_util/src/dsp/envelopes.h"

using namespace imagiro;

class ChorusProcessor : public Processor {
public:
    ChorusProcessor()
        : Processor(ChorusProcessorParameters::PARAMETERS_YAML) {
        rateParam = getParameter("rate");
        depthParam = getParameter("depth");
        mixParam = getParameter("mix");
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override {
        Processor::prepareToPlay(sampleRate, samplesPerBlock);

        maxDelayDepthSamples = maxDelayDepthSeconds * sampleRate;
        channelDelays.clear();
        channelLFOs.clear();
        for (auto i=0; i<getTotalNumOutputChannels(); i++) {
            channelDelays.emplace_back();
            channelDelays.back().resize(maxDelayDepthSamples);

            channelLFOs.emplace_back();
            channelLFOs.back().setPhase(i / (float)getTotalNumOutputChannels());
        }

    }

    void process(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) override {


        for (auto s=0; s<buffer.getNumSamples(); s++) {
            const auto depthSamples = depthParam->getSmoothedValue(s) * maxDelayDepthSamples;
            const auto rate = rateParam->getSmoothedValue(s);
            const auto mix = mixParam->getSmoothedValue(s);

            for (auto c=0; c<buffer.getNumChannels(); c++) {
                channelLFOs[c].set(0, 1, rate / getSampleRate(), 0.3, 0.3);
                auto delaySamples = channelLFOs[c].next() * (depthSamples - 1) + 1;

                auto input = buffer.getSample(c, s);
                auto delayedSample = channelDelays[c].write(input).read(delaySamples);

                buffer.setSample(c, s,
                    input * (1-mix) + delayedSample * mix);
            }

        }
    }

private:
    Parameter *rateParam;
    Parameter *depthParam;
    Parameter *mixParam;

    std::vector<signalsmith::delay::Delay<float>> channelDelays;
    std::vector<signalsmith::envelopes::CubicLfo> channelLFOs;

    const float maxDelayDepthSeconds = 0.06;
    int maxDelayDepthSamples = maxDelayDepthSeconds * 44100;
};
