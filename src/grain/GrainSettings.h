//
// Created by August Pemberton on 03/02/2025.
//

#pragma once
#include "imagiro_processor/src/dsp/interpolation.h"

struct LoopSettings {
    float loopStart = 0;
    float loopLength = 0;
    float loopCrossfade = 0;
    bool loopActive = false;

    int getLoopStartSample(int bufferLength) const {
        return loopStart * bufferLength + INTERP_PRE_SAMPLES;
    }

    int getLoopLengthSamples(int bufferLength) const {
        const int len = loopLength * bufferLength - INTERP_POST_SAMPLES - INTERP_PRE_SAMPLES;
        return std::max(5, len);
    }

    int getLoopEndSample(int bufferLength) const {
        return getLoopStartSample(bufferLength) + getLoopLengthSamples(bufferLength);
    }

    int getCrossfadeSamples(int bufferLength) const {
        return loopCrossfade * getLoopLengthSamples(bufferLength) * 0.5;
    }

    int getCrossfadeStartSample(int bufferLength) const {
        return getLoopEndSample(bufferLength) - getCrossfadeSamples(bufferLength);
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
        return (reverse ? -1.f : 1.f) * std::pow(2.f, (pitch) / 12.f);
    }
};
