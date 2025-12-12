// BypassMixer.h
#pragma once

#include <vector>
#include <imagiro_util/dsp/delay.h>

namespace imagiro {

class BypassMixer {
public:
    void prepare(double sampleRate, unsigned int numChannels) {
        sampleRate_ = sampleRate;
        numChannels_ = numChannels;

        delayLines_.resize(numChannels);
        for (auto& delay : delayLines_) {
            delay.resize(maxDelaySamples_);
            delay.reset();
        }

        // Simple one-pole smoothing
        smoothingCoeff_ = 1.0f - std::exp(-1.0f / (0.05f * static_cast<float>(sampleRate)));
        currentBypassGain_ = targetBypassGain_;
        currentMixGain_ = targetMixGain_;
    }

    void setLatency(int samples) {
        latencySamples_ = samples;
    }

    void setBypass(bool bypassed) {
        targetBypassGain_ = bypassed ? 0.f : 1.f;
    }

    void setMix(float mix) {
        targetMixGain_ = mix;
    }

    void skipSmoothing() {
        currentBypassGain_ = targetBypassGain_;
        currentMixGain_ = targetMixGain_;
    }

    void pushDry(const juce::AudioSampleBuffer& buffer) {
        std::vector<const float*> inputPtrs(buffer.getNumChannels());
        for (int c = 0; c < buffer.getNumChannels(); c++) {
            inputPtrs[c] = buffer.getReadPointer(c);
        }

        pushDry(inputPtrs.data(), buffer.getNumSamples());
    }

    void pushDry(const float* const* input, int numSamples) {
        for (auto c = 0u; c < numChannels_; c++) {
            for (int s = 0; s < numSamples; s++) {
                delayLines_[c].write(input[c][s]);
            }
        }
    }

    void applyMix(juce::AudioSampleBuffer& buffer) {
        std::vector<float*> outputPtrs(buffer.getNumChannels());
        for (int c = 0; c < buffer.getNumChannels(); c++) {
            outputPtrs[c] = buffer.getWritePointer(c);
        }

        applyMix(outputPtrs.data(), buffer.getNumSamples());
    }

    void applyMix(float** wet, int numSamples) {
        for (int s = 0; s < numSamples; s++) {
            // Smooth gains
            currentBypassGain_ += smoothingCoeff_ * (targetBypassGain_ - currentBypassGain_);
            currentMixGain_ += smoothingCoeff_ * (targetMixGain_ - currentMixGain_);

            const auto wetGain = currentBypassGain_ * currentMixGain_;
            const auto dryGain = 1.f - wetGain;

            for (auto c = 0u; c < numChannels_; c++) {
                const auto drySample = delayLines_[c].read(static_cast<float>(latencySamples_));
                wet[c][s] = wet[c][s] * wetGain + drySample * dryGain;
            }
        }
    }

    bool isProcessingNeeded() const {
        return currentBypassGain_ > 0.0001f || targetBypassGain_ > 0.0001f;
    }

private:
    std::vector<signalsmith::delay::Delay<float>> delayLines_;
    int maxDelaySamples_{48000 * 4};
    int latencySamples_{0};
    unsigned int numChannels_{2};
    double sampleRate_{48000.0};

    float targetBypassGain_{1.f};
    float targetMixGain_{1.f};
    float currentBypassGain_{1.f};
    float currentMixGain_{1.f};
    float smoothingCoeff_{0.01f};
};

} // namespace imagiro