//
// Created by August Pemberton on 08/05/2025.
//

#pragma once
#include "imagiro_processor/src/Processor.h"
#include "imagiro_util/src/dsp/envelopes.h"
#include "Parameters.h"
#include "imagiro_processor/src/dsp/LFO.h"
#include "imagiro_util/src/dsp/delay.h"

using namespace imagiro;

class WobbleProcessor : public Processor {
public:
    WobbleProcessor()
        : Processor(WobbleProcessorParameters::PARAMETERS_YAML,
                    ParameterLoader(), getDefaultProperties()) {
        rateParam = getParameter("rate");
        depthParam = getParameter("depth");
        dampParam = getParameter("damp");
    }

    BusesProperties getDefaultProperties() {
        return BusesProperties()
                .withInput("Input", juce::AudioChannelSet::stereo(), true)
                .withOutput("Output", juce::AudioChannelSet::stereo(), true);
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override {
        Processor::prepareToPlay(sampleRate, samplesPerBlock);

        channelDelays.clear();
        for (auto i = 0; i < getTotalNumOutputChannels(); i++) {
            channelDelays.emplace_back();
            channelDelays.back().resize(sampleRate * wobbleDepthRange.end * 2);
        }
    }

    void processWobble() {
        auto wobbleLength = wobbleLengthRange.convertFrom0to1(
            dampParam->getProcessorValue());

        auto wobbleDepth = wobbleDepthRange.convertFrom0to1(
            depthParam->getProcessorValue());

        std::normal_distribution<float> lengthDistribution(wobbleLength, wobbleLength / 5);
        auto wobbleLength_r = juce::Range<float>(0.01f, 5).clipValue(lengthDistribution(generator));

        std::normal_distribution<float> depthDistribution(wobbleDepth, wobbleDepth / 5);
        auto wobbleDepth_r = juce::Range<float>(0.f, 0.15f).clipValue(depthDistribution(generator));

        phasePerSample = 1 / (wobbleLength_r * getSampleRate());
        currentWobbleDepth = wobbleDepth_r * getSampleRate() * wobbleLength_r; // multiply by length so pitch variation is constant
        wobblePhase = 0;
    }

    float generateTimeToNextWobble() const {
        auto rate = rateParam->getProcessorValue();
        auto min = wobbleTimeMinRange.convertFrom0to1(1 - rate);
        auto max = wobbleTimeMaxRange.convertFrom0to1(1 - rate);

        return juce::Random::getSystemRandom().nextFloat() * (max - min) + min;
    }


    void process(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) override {
        for (auto s = 0; s < buffer.getNumSamples(); s++) {
            if (wobbleCounter > 1) {
                wobbleCounter--;
                continue;
            }

            if (wobbleCounter == 1) {
                wobbleCounter = 0;
                processWobble();
            }

            auto phase = wobblePhase * juce::MathConstants<float>::twoPi;
            phase -= juce::MathConstants<float>::halfPi; // to start and end at lowest value
            if (phase > juce::MathConstants<float>::pi) phase -= juce::MathConstants<float>::twoPi;
            auto delaySamples = (fastsin(phase) / 2 + 0.5) * currentWobbleDepth;

            for (auto c = 0; c < buffer.getNumChannels(); c++) {
                auto input = buffer.getSample(c, s);
                auto delayedSample = channelDelays[c].write(input).read(delaySamples);
                buffer.setSample(c, s, delayedSample);
            }

            wobblePhase += phasePerSample;
            if (wobblePhase >= 1.f) {
                wobbleCounter = generateTimeToNextWobble() * getSampleRate();
            }
            wobblePhase = std::clamp(wobblePhase, 0.f, 1.f);
        }
    }

private:
    Parameter *rateParam;
    Parameter *depthParam;
    Parameter *dampParam;

    std::vector<signalsmith::delay::Delay<float>> channelDelays;

    int wobbleCounter = 0;

    float wobblePhase {1};
    float currentWobbleDepth {0};
    float phasePerSample {0};

    std::default_random_engine generator;

    juce::NormalisableRange<float> wobbleTimeMinRange{0.01f, 4.f, 0.f, 0.3f};
    juce::NormalisableRange<float> wobbleTimeMaxRange{0.07f, 6.f, 0.f, 0.35f};
    juce::NormalisableRange<float> wobbleLengthRange{0.04f, 1.f, 0.f, 0.8f};
    juce::NormalisableRange<float> wobbleDepthRange{0.0f, 0.08f, 0.f, 0.2f};
};
