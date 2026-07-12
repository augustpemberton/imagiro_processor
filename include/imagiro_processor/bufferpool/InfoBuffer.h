//
// Created by August Pemberton on 14/07/2025.
//

#pragma once
#include "AudioBuffer.h"
#include <filesystem>

namespace imagiro {

struct InfoBuffer {
    AudioBuffer buffer;
    double sampleRate;
    float maxMagnitude;
    std::filesystem::path file;
};

} // namespace imagiro
