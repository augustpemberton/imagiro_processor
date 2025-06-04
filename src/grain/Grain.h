//
// Created by August Pemberton on 15/08/2022.
//

#pragma once
#include "GrainBuffer.h"
#include "GrainSettings.h"
#include <juce_dsp/juce_dsp.h>
#include <imagiro_util/imagiro_util.h>

class GrainStream;

class Grain {
public:
    struct Serialized {
        int position;
        int loopFadePosition;
        float loopFadeProgress;
        float gain;
        float pitchRatio;

        choc::value::Value getState() const {
            auto state = choc::value::createObject("Grain");
            state.addMember("position", position);
            state.addMember("loopFadePosition", loopFadePosition);
            state.addMember("loopFadeProgress", loopFadeProgress);
            state.addMember("gain", gain);
            state.addMember("pitchRatio", pitchRatio);
            return state;
        }
    };

    Serialized serialize() {
        return {
                static_cast<int>(getPointer()),
                static_cast<int>(getLoopFadePointer()),
                getLoopFadeProgress(),
                getCurrentGain(),
                getCurrentPitchRatio()
        };
    }

    Grain(GrainBuffer& buffer, size_t indexInStream = 0);
    Grain(const Grain&) = delete;

    void configure(GrainSettings settings);
    GrainSettings& getGrainSettings() { return settings; }

    void play(int delay = 0);
    void stop(bool fadeout = false);

    bool processBlock(juce::AudioSampleBuffer& out, int startSample, int numSamples, bool setNotAdd = false);
    void prepareToPlay(double sampleRate, int maxBlockSize);

    double getPointer() const { return pointer; }
    double getLoopFadePointer() const { return isLooping ? loopFadePointer : -1; }
    float getLoopFadeProgress() const { return isLooping ? loopFadeProgress : 0; }
    bool isPlaying() const { return playing; }

    struct Listener {
        virtual void OnGrainStarted (Grain&) {}
        virtual void OnGrainFinished(Grain&) {}
    };

    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

    int getSamplesUntilStart() const { return samplesUntilStart; }
    int getSamplesUntilEndOfBuffer();

    void setPitch(float pitch, bool skipSmoothing = false) {
        settings.pitch = pitch;
        if (skipSmoothing) smoothPitchRatio.setCurrentAndTargetValue(settings.getPitchRatio());
        else smoothPitchRatio.setTargetValue(settings.getPitchRatio());
    }

    float getCurrentGain() { return settings.gain * windowFunction(progress); }
    float getCurrentPitchRatio() { return smoothPitchRatio.getCurrentValue(); }
    size_t getIndexInStream() { return indexInStream; }

    GrainBuffer& getGrainBuffer() { return grainBuffer; }

    void updateLoopSettings(LoopSettings settings);
    void updatePan(float val) { smoothedPan.setTargetValue(val); }

private:
    const size_t indexInStream;
    juce::ListenerList<Listener> listeners;

    bool isLooping;
    std::optional<LoopSettings> queuedLoopSettings;

    juce::dsp::LookupTableTransform<float> sinApprox {
        [] (float x) { return (float)std::sin(x); },
        -6.5f, 6.5f, 80};

    juce::dsp::LookupTableTransform<float> windowFunction;
    static float getGrainShapeGain(float p, float sym, float skew);

    float getGrainSpeed();

    juce::SmoothedValue<float> smoothedPan {0.5};
    float* calculatePanCoeffs(float val);
    float cachedPanCoeffs [2];
    float cachedPan {0};
    float spreadVal {0};

    GrainBuffer& grainBuffer;
    std::shared_ptr<juce::AudioSampleBuffer> currentBuffer;

    GrainSettings settings;

    juce::SmoothedValue<float> smoothPitchRatio;
    float gain;

    double pointer;
    double loopFadePointer {-1}; // for UI to display both fade out and fade in of loop
    float loopFadeProgress {0};
    float progress {0};
    float progressPerSample {0};
    double sampleRate;

    void setNewLoopSettingsInternal(LoopSettings loopSettings);

    const float quickfadeSeconds {0.05f};
    int quickfadeSamples {0};
    float quickfadeGain {0};
    float quickfadeGainPerSample {0};
    bool quickfading {false};

    bool playing {false};
    bool firstBlockFlag {false};
    bool stopFlag {false};

    int samplesUntilStart {0};

    juce::AudioSampleBuffer temp;
};


