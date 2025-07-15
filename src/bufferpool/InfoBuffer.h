//
// Created by August Pemberton on 14/07/2025.
//

#pragma once
#include "juce_core/juce_core.h"

struct InfoBuffer {
    juce::AudioSampleBuffer buffer;
    double sampleRate;
    float maxMagnitude;
    juce::File file;
};
