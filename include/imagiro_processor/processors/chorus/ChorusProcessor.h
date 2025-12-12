//
// Created by August Pemberton on 08/05/2025.
//

#pragma once
#include "../../processor/Processor.h"
#include "Parameters.h"
#include "imagiro_util/src/dsp/envelopes.h"
#include <vector>

using namespace imagiro;

class ChorusProcessor : public Processor {
public:
    ChorusProcessor()
        : Processor(ChorusProcessorParameters::PARAMETERS_YAML, ParameterLoader(), getDefaultProperties()) {
        rateParam = getParameter("rate");
        depthParam = getParameter("depth");
        feedbackParam = getParameter("feedback");
        voicesParam = getParameter("voices");
    }

    BusesProperties getDefaultProperties() {
        return BusesProperties()
            .withInput("Input", juce::AudioChannelSet::stereo(), true)
            .withOutput("Output", juce::AudioChannelSet::stereo(), true);
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override {
        Processor::prepareToPlay(sampleRate, samplesPerBlock);

        maxDelayDepthSamples = maxDelayDepthSeconds * sampleRate;
        
        const int maxVoices = voicesParam->getNormalisableRange().end;
        
        voiceDelays.clear();
        voiceLFOs.clear();
        
        // Initialize for each channel
        for (auto c = 0; c < getTotalNumOutputChannels(); c++) {
            voiceDelays.emplace_back();
            voiceLFOs.emplace_back();
            
            for (auto v = 0; v < maxVoices; v++) {
                voiceDelays[c].emplace_back();
                voiceDelays[c][v].resize(maxDelayDepthSamples);
                
                voiceLFOs[c].emplace_back();
                // Offset phase slightly for each voice
                float phaseOffset = c / (float)getTotalNumOutputChannels() + v * 0.1f;
                voiceLFOs[c][v].setPhase(phaseOffset);
            }
        }
    }

    void process(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) override {
        for (auto s = 0; s < buffer.getNumSamples(); s++) {
            const auto rate = rateParam->getSmoothedValue(s);

            auto depthSkewed = fastpow(depthParam->getSmoothedValue(s), 1.7);

            // normalize depth to rate
            // otherwise high rate has higher depth, as we are moving the delay line faster
            depthSkewed /= rate;

            // clamp depth to 1, otherwise we'll go over the max delay depth
            depthSkewed = std::min(1.f, depthSkewed);

            auto depthSamples = depthSkewed * maxDelayDepthSamples;
            const auto feedback = feedbackParam->getSmoothedValue(s);

            const int numVoices = static_cast<int>(voicesParam->getSmoothedValue(s) + 0.5f);
            
            for (auto c = 0; c < buffer.getNumChannels(); c++) {
                const auto input = buffer.getSample(c, s);
                float wetSignal = 0.0f;

                // Process each active voice
                for (auto v = 0; v < numVoices; v++) {
                    voiceLFOs[c][v].set(0, 1, rate / getSampleRate(), 0.01, 0.05);

                    // Get delay time from LFO
                    auto delaySamples = voiceLFOs[c][v].next() * (depthSamples - 1) + 1;

                    // Read from delay line
                    auto delayedSample = voiceDelays[c][v].read(delaySamples);

                    // Add to wet signal (without division at this point)
                    wetSignal += delayedSample;

                    // Write input + feedback to delay line
                    voiceDelays[c][v].write(input + delayedSample * feedback);
                }

                // Calculate adaptive gain compensation based on depth and voices
                float depthNormalized = depthParam->getSmoothedValue(s); // 0.0 to 1.0
                // This formula creates a curve where:
                // - At depth=0: compensationFactor = numVoices (full compensation)
                // - At depth=1: compensationFactor = sqrt(numVoices) (reduced compensation)
                float compensationFactor = numVoices * (1.0f - depthNormalized) +
                                          std::sqrt(static_cast<float>(numVoices)) * depthNormalized;

                // Apply the adaptive gain compensation
                wetSignal /= compensationFactor;
                
                buffer.setSample(c, s, wetSignal);
            }
        }
    }

private:
    Parameter *rateParam;
    Parameter *depthParam;
    Parameter *feedbackParam;
    Parameter *voicesParam;

    std::vector<std::vector<signalsmith::delay::Delay<float>>> voiceDelays;
    std::vector<std::vector<signalsmith::envelopes::CubicLfo>> voiceLFOs;

    const float maxDelayDepthSeconds = 0.06;
    int maxDelayDepthSamples = maxDelayDepthSeconds * 44100;
};