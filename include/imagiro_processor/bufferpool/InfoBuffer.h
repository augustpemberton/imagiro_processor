//
// Created by August Pemberton on 14/07/2025.
//

#pragma once
#include "juce_audio_basics/juce_audio_basics.h"

namespace imagiro {

struct InfoBuffer {
    juce::AudioSampleBuffer buffer;
    double sampleRate;
    float maxMagnitude;
    juce::File file;
};

} // namespace imagiro
