//
// Created by August Pemberton on 09/09/2023.
//

#include "TransientDetector.h"

namespace imagiro {
    TransientDetector::TransientDetector(float sensitivity, float lookaheadSeconds)
        : sampleRate(0), lookaheadSeconds(lookaheadSeconds),
          fastEnv(getFastEnv(sensitivity)),
          slowEnv((getSlowEnv(sensitivity))),
          lookaheadEnv(lookaheadSeconds * 1000, 50) {
    }

    std::vector<Transient> TransientDetector::getTransients(const juce::AudioSampleBuffer &buffer, float dbThreshold) {
        std::vector<Transient> transients;
        int lookaheadSamples = (int)(lookaheadSeconds * (float)sampleRate / (float)undersampleFactor);

        // push initial sample a few times
        for (int i=0; i<10; i++) {
            fastEnv.pushSample(buffer.getMagnitude(0, 1));
            slowEnv.pushSample(buffer.getMagnitude(0, 1));
            lookaheadEnv.pushSample(buffer.getMagnitude(0, 1));
        }

        bool readyForNextTransient = true;

        auto maxSample = std::max(0.0001f, buffer.getMagnitude(0, buffer.getNumSamples()));

        for (auto i=0; i<buffer.getNumSamples(); i+=undersampleFactor) {
            auto max = abs(buffer.getSample(0, std::min(i, buffer.getNumSamples()-1)));
            auto lookaheadMax = abs(buffer.getSample(0, std::min(i+lookaheadSamples, buffer.getNumSamples()-1)));

            max /= maxSample;
            lookaheadMax /= maxSample;

            auto look = lookaheadEnv.pushSample(lookaheadMax);

            auto fast = fastEnv.pushSample(max);
            auto slow = slowEnv.pushSample(max);

            auto r = juce::Decibels::gainToDecibels(fast) - juce::Decibels::gainToDecibels(slow);
            r += juce::Decibels::gainToDecibels(look) / 10;
            auto diff = juce::Decibels::gainToDecibels(fast - slow);

            if (readyForNextTransient && i != 0) {
                if (r > dbThreshold && diff > -60.0f) {
                    auto transientSample = std::max(0, (int)(i));
                    transients.emplace_back(transientSample, r);
                    readyForNextTransient = false;
                }
            } else {
                if (slow > fast) {
                    readyForNextTransient = true;
                }
            }
        }

        return transients;
    }

    void TransientDetector::setUndersampleFactor(int undersample) {
        undersampleFactor = undersample;
        updateSampleRates();
    }

    void TransientDetector::setSampleRate(float sr) {
        sampleRate = sr;
        updateSampleRates();
    }

    EnvelopeFollower<float> TransientDetector::getFastEnv(float sensitivity) {
        auto invSens = 1 - sensitivity;
        return EnvelopeFollower<float>(2.f + invSens * 80.f, 10.f + sensitivity * 100.f);
    }

    EnvelopeFollower<float> TransientDetector::getSlowEnv(float sensitivity) {
        return EnvelopeFollower<float>(100.f + sensitivity * 500.f, 300.f + sensitivity * 500.f);
    }

    void TransientDetector::updateSampleRates() {
        fastEnv.setSampleRate(sampleRate / undersampleFactor);
        slowEnv.setSampleRate(sampleRate / undersampleFactor);
        lookaheadEnv.setSampleRate(sampleRate / undersampleFactor);
    }

}