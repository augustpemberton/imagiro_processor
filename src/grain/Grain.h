//
// Created by August Pemberton on 15/08/2022.
//

#pragma once
#include "GrainSettings.h"
#include <juce_dsp/juce_dsp.h>

#include "GrainSampleData.h"
#include "imagiro_processor/src/bufferpool/FileBufferCache.h"

class Grain {
public:
    struct Serialized {
        int position { -1 };
        int loopFadePosition;
        float loopFadeProgress;
        float gain;
        float pitchRatio;
    };

    Serialized serialize() const {
        return {
                static_cast<int>(getPointer()),
                static_cast<int>(getLoopFadePointer()),
                getLoopFadeProgress(),
                getCurrentGain(),
                getCurrentPitchRatio()
        };
    }

    explicit Grain(std::vector<GrainSampleData>& data, size_t indexInStream = 0);
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
    int getSamplesUntilEndOfBuffer() const;

    void setPitch(const float pitch, const bool skipSmoothing = false) {
        settings.pitch = pitch;
        if (skipSmoothing) smoothPitchRatio.setCurrentAndTargetValue(settings.getPitchRatio());
        else smoothPitchRatio.setTargetValue(settings.getPitchRatio());
    }

    float getCurrentGain() const { return settings.gain * windowFunction(progress); }
    float getCurrentPitchRatio() const { return smoothPitchRatio.getCurrentValue(); }
    size_t getIndexInStream() const { return indexInStream; }

    void updateLoopSettings(LoopSettings settings, bool force = false);
    void updatePan(const float val) { smoothedPan.setTargetValue(val); }

    void resetBuffer();

    void setBuffer(const std::shared_ptr<InfoBuffer>& buf);
    const std::shared_ptr<InfoBuffer>& getBuffer() { return currentBuffer; }

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

    std::vector<GrainSampleData>& sampleDataBuffer;

    juce::dsp::LookupTableTransform<float> sinApprox {
        [] (const float x) { return std::sin(x); },
        -6.5f, 6.5f, 80};

    juce::dsp::LookupTableTransform<float> windowFunction;
    static float getGrainShapeGain(float p, float sym, float skew);

    float getGrainSpeed() const;

    juce::SmoothedValue<float> smoothedPan {0.5};
    float* calculatePanCoeffs(float val);
    float cachedPanCoeffs [2];
    float cachedPan {0};
    float spreadVal {0};

    std::shared_ptr<InfoBuffer> currentBuffer;

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
};


