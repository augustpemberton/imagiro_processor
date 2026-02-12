#pragma once

#include "adsr/ADSR.h"
#include "adsr/ADSRParameters.h"

#include <juce_core/juce_core.h>

class Envelope {
public:
    Envelope() = default;

    void setSampleRate(double rate) {
        sampleRate = rate;
        quickfadeRate = static_cast<float>(1.0f / (quickfadeSeconds * sampleRate));
        quickfadeRate *= downsampleRate;
        if (paramsDirty_) {
            params = targetParams_;
            paramsDirty_ = false;
        }
        applyParamsToADSR();
    }

    void setParameters(const ADSRParameters& p) {
        if (!isActive() && !quickfading) {
            params = p;
            applyParamsToADSR();
            return;
        }
        targetParams_ = p;
        paramsDirty_ = true;
    }

    void noteOn() { adsr.setNoteOn(true); }
    void noteOff() { adsr.setNoteOn(false); }
    void quickFadeOut() { quickfading = true; }
    bool isQuickfadingOut() const { return quickfading; }
    int getQuickfadeLengthSamples() const { return static_cast<int>(quickfadeSeconds * sampleRate); }
    bool isActive() const { return adsr.getState() != ADSR::EnvState::env_idle; }
    float getCurrentLevel() const { return outputLevel; }

    void reset() {
        adsr.reset();
        quickfading = false;
        outputLevel = 0.f;
        targetLevel = 0.f;
        lastTargetLevel = 0.f;
        downsampleCounter = 0.f;
        if (paramsDirty_) {
            params = targetParams_;
            applyParamsToADSR();
            paramsDirty_ = false;
        }
    }

    void setMaxBlockSize(int samples) {
        envTempBuffer.setSize(1, samples);
    }

    forcedinline float getNextSample() {
        downsampleCounter += downsampleInv;

        if (downsampleCounter >= 1.f) {
            downsampleCounter = 0.f;
            lastTargetLevel = targetLevel;

            if (paramsDirty_) smoothParamsStep();

            targetLevel = quickfading ? (targetLevel - quickfadeRate) : adsr.process();

            if (quickfading && targetLevel < 0.f) {
                targetLevel = 0.f;
                quickfading = false;
                adsr.reset();
            }
        }

        outputLevel = lastTargetLevel + (targetLevel - lastTargetLevel) * downsampleCounter;
        return outputLevel;
    }

    void applyToBuffer(juce::AudioSampleBuffer& buffer, int numChannels, int numSamples) {
        // Fast path: in sustain phase with settled level, output is constant
        if (!quickfading && !paramsDirty_
            && adsr.getState() == ADSR::EnvState::env_sustain
            && lastTargetLevel == targetLevel) {
            outputLevel = targetLevel;
            for (int c = 0; c < numChannels; c++) {
                juce::FloatVectorOperations::multiply(
                    buffer.getWritePointer(c), targetLevel, numSamples);
            }
            return;
        }

        float* __restrict envOut = envTempBuffer.getWritePointer(0);

        for (int i = 0; i < numSamples; i++) {
            envOut[i] = getNextSample();
        }

        for (int c = 0; c < numChannels; c++) {
            juce::FloatVectorOperations::multiply(
                buffer.getWritePointer(c), envOut, numSamples);
        }
    }

private:
    juce::AudioSampleBuffer envTempBuffer;
    ADSR adsr;
    ADSRParameters params;

    float lastTargetLevel {0};
    float outputLevel {0};
    float targetLevel {0};

    double sampleRate {0};

    const float quickfadeSeconds {0.04f};
    float quickfadeRate {0};
    bool quickfading {false};

    const int downsampleRate = 16;
    const float downsampleInv = 1.f / static_cast<float>(downsampleRate);
    float downsampleCounter = 0;

    ADSRParameters targetParams_;
    bool paramsDirty_ {false};
    static constexpr float paramSmoothAlpha_ = 0.1f;

    void applyParamsToADSR() {
        adsr.setAttackRate(params.attack * sampleRate / downsampleRate);
        adsr.setDecayRate(params.decay * sampleRate / downsampleRate);
        adsr.setReleaseRate(params.release * sampleRate / downsampleRate);
        adsr.setSustainLevel(params.sustain);
        adsr.setTargetRatioA(0.3f * params.attack);
        adsr.setTargetRatioDR(0.02f);
    }

    void smoothParamsStep() {
        bool converged = true;
        auto smoothOne = [&](float& current, float target) {
            float diff = target - current;
            if (std::abs(diff) < 0.0001f) {
                current = target;
            } else {
                current += diff * paramSmoothAlpha_;
                converged = false;
            }
        };
        smoothOne(params.attack, targetParams_.attack);
        smoothOne(params.decay, targetParams_.decay);
        smoothOne(params.sustain, targetParams_.sustain);
        smoothOne(params.release, targetParams_.release);
        applyParamsToADSR();
        if (converged) paramsDirty_ = false;
    }
};
