//
// Created by August Pemberton on 03/02/2025.
//

#pragma once
#include "imagiro_processor/src/dsp/interpolation.h"

using namespace imagiro;

struct LoopSettings {
    float loopStart = 0;
    float loopLength = 0;
    float loopCrossfade = 0;
    bool loopActive = false;

    bool operator==(const LoopSettings &other) const {
        return this->loopStart == other.loopStart &&
               this->loopLength == other.loopLength &&
               this->loopCrossfade == other.loopCrossfade &&
               this->loopActive == other.loopActive;
    }

    int getLoopStartSample(const int bufferLength) const {
        return std::max(static_cast<int>(loopStart * bufferLength), INTERP_PRE_SAMPLES);
    }

    int getLoopLengthSamples(const int bufferLength) const {
        const int len = loopLength * bufferLength;
        const auto maxLength = bufferLength - getLoopStartSample(bufferLength) - INTERP_POST_SAMPLES;
        return std::clamp(len, 4, maxLength);
    }

    int getLoopEndSample(const int bufferLength) const {
        return getLoopStartSample(bufferLength) + getLoopLengthSamples(bufferLength);
    }

    int getCrossfadeSamples(const int bufferLength) const {
        return loopCrossfade * getLoopLengthSamples(bufferLength) * 0.5;
    }

    int getCrossfadeStartSample(const int bufferLength) const {
        return getLoopEndSample(bufferLength) - getCrossfadeSamples(bufferLength);
    }

    int getReverseCrossfadeStartSample(const int bufferLength) const {
        return getLoopStartSample(bufferLength) + getCrossfadeSamples(bufferLength);
    }

    static LoopSettings fromParameters(const Parameter& loopStart, const Parameter& loopLength,
        const Parameter& loopCrossfade, const Parameter& loopActive, const size_t voiceIndex) {
        LoopSettings l;
        l.loopStart = loopStart.getProcessorValue(voiceIndex);
        l.loopLength = loopLength.getProcessorValue(voiceIndex);
        l.loopCrossfade = loopCrossfade.getProcessorValue(voiceIndex);
        l.loopActive = loopActive.getBoolValue();

        l.loopStart = std::min(l.loopStart, 0.99f);
        l.loopLength = std::min(1 - l.loopStart, l.loopLength);
        return l;
    }

};

struct GrainSettings {
    float duration = 0.5f;
    float position = 0;

    LoopSettings loopSettings;

    float gain = 1.f;
    float pitch = 0;
    float pan = 0.5f;
    float spread = 0.f;
    float shape = 0.5f;
    float skew = 0.5f;
    bool reverse = false;

    float getPitchRatio() const {
        return (reverse ? -1.f : 1.f) * std::pow(2.f, pitch / 12.f);
    }
};
