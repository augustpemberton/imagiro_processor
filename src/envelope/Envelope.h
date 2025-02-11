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

        setParameters(params);
    }

    void setParameters(const ADSRParameters& p) {
        params = p;
        adsr.setAttackRate(params.attack * sampleRate / downsampleRate);
        adsr.setDecayRate(params.decay * sampleRate / downsampleRate);
        adsr.setReleaseRate(params.release * sampleRate / downsampleRate);
        adsr.setSustainLevel(params.sustain);

        adsr.setTargetRatioA(0.01f);
        adsr.setTargetRatioDR(0.01f);
    }

    void noteOn() {
        adsr.setNoteOn(true);
    }

    void noteOff() {
        adsr.setNoteOn(false);
    }

    void quickFadeOut() {
        quickfading = true;
    }

    bool isQuickfadingOut() const {
        return quickfading;
    }

    int getQuickfadeLengthSamples() const {
        return static_cast<int>(quickfadeSeconds * sampleRate);
    }

    float getNextSample() {
        downsampleCounter += downsampleInv;

        if (downsampleCounter >= 1) {
            downsampleCounter = 0;

            float newLevel = quickfading ? (outputLevel - quickfadeRate) : adsr.process();

            if (newLevel < 0.f && quickfading) {
                quickfading = false;
                adsr.reset();
                return 0.f;
            }

            lastTargetLevel = targetLevel;
            targetLevel = newLevel;
        }

        auto newOutput = imagiro::lerp(lastTargetLevel, targetLevel, downsampleCounter);
        outputLevel = newOutput;
        return outputLevel;
    }

    bool isActive() const {
        return adsr.getState() != ADSR::EnvState::env_idle;
    }

    void reset() {
        adsr.reset();
        quickfading = false;
    }

    void applyToBuffer(juce::AudioSampleBuffer& buffer, int numChannels, int numSamples) {
        auto eW = envTempBuffer.getWritePointer(0);

        // Generate envelope values
        for (auto s = 0; s < numSamples; s++) {
            *eW++ = getNextSample();
        }

        // Apply buffer
        for (auto c = 0; c < numChannels; c++) {
            juce::FloatVectorOperations::multiply(buffer.getWritePointer(c),
                                                  envTempBuffer.getReadPointer(0),
                                                  numSamples);
        }
    }

    float getCurrentLevel() const {
        return outputLevel;
    }

    void setMaxBlockSize(int samples) {
        envTempBuffer.setSize(1, samples);
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

    const int downsampleRate = 8;
    const float downsampleInv = 1.f / (float) downsampleRate;
    float downsampleCounter = 0;

    float prevEnv {0};
    float nextEnv {0};

};
