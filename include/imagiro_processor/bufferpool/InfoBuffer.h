//
// Created by August Pemberton on 14/07/2025.
//

#pragma once
#include "AudioBuffer.h"
#include <imagiro_util/fs.h>

namespace imagiro {

struct InfoBuffer {
    AudioBuffer buffer;
    double sampleRate;
    float maxMagnitude;
    imagiro::fs::path file;
};

} // namespace imagiro
