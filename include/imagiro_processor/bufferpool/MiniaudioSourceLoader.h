#pragma once

#include "ISourceLoader.h"

namespace imagiro {

// JUCE-free ISourceLoader backed by miniaudio's decoders (wav/flac/mp3/ogg).
// Decodes the whole file to deinterleaved f32 at native channel count and
// sample rate, matching the observable behavior of the JUCE FileBufferCache
// load path (native channels, native sample rate, no normalization).
class MiniaudioSourceLoader : public ISourceLoader {
public:
    std::shared_ptr<InfoBuffer> load(
        const imagiro::fs::path& path,
        const std::function<bool()>& shouldCancel) override;
};

} // namespace imagiro
