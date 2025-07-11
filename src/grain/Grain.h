//
// Created by August Pemberton on 15/08/2022.
//

#pragma once
#include "GrainBuffer.h"
#include "GrainSettings.h"
#include <juce_dsp/juce_dsp.h>
#include <imagiro_util/imagiro_util.h>

#include "imagiro_processor/src/dsp/DCBlocker.h"
#include "imagiro_processor/src/dsp/filter/CascadedBiquadFilter.h"

class Grain {
public:
    struct Serialized {
        int position { -1 };
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

    void processBlock(juce::AudioSampleBuffer& out, int startSample, int numSamples, bool setNotAdd = false);
    void prepareToPlay(double sampleRate, int maxBlockSize);

    double getPointer() const { return pointer; }
    double getLoopFadePointer() const { return isLooping ? loopFadePointer : -1; }
    float getLoopFadeProgress() const { return isLooping ? loopFadeProgress : 0; }
    bool isPlaying() const { return playing; }

    struct Listener {
        virtual ~Listener() = default;

        virtual void OnGrainStarted (Grain&) {}
        virtual void OnGrainFinished(Grain&) {}
    };

    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

    int getSamplesUntilStart() const { return samplesUntilStart; }
    int getSamplesUntilEndOfBuffer();

    /*
     * note - we can update stream pitch every block, but grain pitch only gets set once
     * (is this really what we want? pitch bend, detune etc? should we be managing this outside of grain?)
     */
    void setStreamPitch(float pitch, bool skipSmoothing = false) {
        settings.streamPitch = pitch;
        if (skipSmoothing) smoothPitchRatio.setCurrentAndTargetValue(settings.getPitchRatio());
        else smoothPitchRatio.setTargetValue(settings.getPitchRatio());
    }

    float getCurrentGain() { return settings.gain * windowFunction(progress); }
    float getCurrentPitchRatio() { return smoothPitchRatio.getCurrentValue(); }
    size_t getIndexInStream() { return indexInStream; }

    GrainBuffer& getGrainBuffer() { return grainBuffer; }

    void updateLoopSettings(LoopSettings settings, bool force = false);
    void updatePan(float val) { smoothedPan.setTargetValue(val); }

private:
    const size_t indexInStream;
    juce::ListenerList<Listener> listeners;

    bool isLooping;
    std::optional<LoopSettings> queuedLoopSettings;

    struct CachedLoopBoundaries {
        int loopStartSample = 0;
        int loopEndSample = 0;
        int loopCrossfadeSamples = 0;
        int loopFadeStart = 0;
        int loopFadeStartReverse = 0;
        int loopLengthSamples = 0;
    } cachedLoopBoundaries;
    void updateCachedLoopBoundaries();

    struct SampleData {
        double position;
        bool looping;
        float loopFadeProgress;
        double loopFadePointer;
    };
    std::vector<SampleData> sampleDataBuffer;

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
    std::shared_ptr<MipmappedBuffer<>> currentMipMapBuffer;

    GrainSettings settings;

    juce::SmoothedValue<double> smoothPitchRatio;
    double sampleRateRatio {1};
    void updateSampleRateRatio();
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


