#include "MiniaudioSourceLoader.h"

// Declarations only — the implementation lives in miniaudio_impl.cpp, which is
// the single MA_IMPLEMENTATION TU in the imagiro_audio_decode static lib.
#include "../miniaudio/miniaudio.h"

#include <cmath>
#include <vector>

namespace imagiro {

std::shared_ptr<InfoBuffer> MiniaudioSourceLoader::load(
    const imagiro::fs::path& path,
    const std::function<bool()>& shouldCancel)
{
    // Decode to f32, native channel count and sample rate (0 == keep source's).
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);

    ma_decoder decoder;
    if (ma_decoder_init_file(path.string().c_str(), &config, &decoder) != MA_SUCCESS)
        return nullptr;

    ma_format format;
    ma_uint32 channels = 0;
    ma_uint32 sampleRate = 0;
    if (ma_decoder_get_data_format(&decoder, &format, &channels, &sampleRate,
                                   nullptr, 0) != MA_SUCCESS
        || channels == 0) {
        ma_decoder_uninit(&decoder);
        return nullptr;
    }

    constexpr ma_uint64 kChunkFrames = 8192;
    std::vector<float> interleaved;
    std::vector<float> chunk(static_cast<size_t>(kChunkFrames) * channels);

    // Preallocate when the length is known (unknown for some streamed formats).
    ma_uint64 lengthFrames = 0;
    if (ma_decoder_get_length_in_pcm_frames(&decoder, &lengthFrames) == MA_SUCCESS
        && lengthFrames > 0)
        interleaved.reserve(static_cast<size_t>(lengthFrames) * channels);

    for (;;) {
        if (shouldCancel && shouldCancel()) {
            ma_decoder_uninit(&decoder);
            return nullptr;
        }

        ma_uint64 framesRead = 0;
        ma_result r = ma_decoder_read_pcm_frames(&decoder, chunk.data(),
                                                 kChunkFrames, &framesRead);
        if (framesRead > 0)
            interleaved.insert(interleaved.end(), chunk.begin(),
                               chunk.begin() + static_cast<size_t>(framesRead) * channels);

        if (framesRead < kChunkFrames || r != MA_SUCCESS)
            break;
    }

    ma_decoder_uninit(&decoder);

    const int numChannels = static_cast<int>(channels);
    const int numFrames = static_cast<int>(interleaved.size() / channels);
    if (numFrames <= 0)
        return nullptr;

    auto info = std::make_shared<InfoBuffer>();
    info->buffer.setSize(numChannels, numFrames);
    info->sampleRate = static_cast<double>(sampleRate);
    info->file = path;

    float maxMagnitude = 0.f;
    for (int ch = 0; ch < numChannels; ++ch) {
        float* dst = info->buffer.getWritePointer(ch);
        const float* src = interleaved.data() + ch;
        for (int i = 0; i < numFrames; ++i) {
            float v = src[static_cast<size_t>(i) * channels];
            dst[i] = v;
            maxMagnitude = std::max(maxMagnitude, std::abs(v));
        }
    }
    info->maxMagnitude = maxMagnitude;

    return info;
}

} // namespace imagiro
