#pragma once

#include <algorithm>
#include <cstring>
#include <vector>

namespace imagiro {

// Owning multichannel float buffer. Provides a small framework-independent
// sample-buffer API used across imagiro plugins.
class AudioBuffer {
public:
    AudioBuffer() = default;

    AudioBuffer(int numChannels, int numSamples) {
        setSize(numChannels, numSamples);
    }

    void setSize(int numChannels, int numSamples, bool keepExistingContent = false,
                 bool clearExtraSpace = false, bool avoidReallocating = false) {
        if (numChannels == numChannels_ && numSamples == numSamples_)
            return;
        if (keepExistingContent) {
            AudioBuffer old = std::move(*this);
            allocate(numChannels, numSamples);
            int copyCh = std::min(numChannels, old.numChannels_);
            int copyN = std::min(numSamples, old.numSamples_);
            for (int ch = 0; ch < copyCh; ch++)
                std::memcpy(getWritePointer(ch), old.getReadPointer(ch),
                            sizeof(float) * static_cast<size_t>(copyN));
            return;
        }
        (void)clearExtraSpace;
        (void)avoidReallocating;
        allocate(numChannels, numSamples);
    }

    int getNumChannels() const { return numChannels_; }
    int getNumSamples() const { return numSamples_; }

    const float* getReadPointer(int channel) const {
        return data_.data() + static_cast<size_t>(channel) * static_cast<size_t>(numSamples_);
    }

    float* getWritePointer(int channel) {
        return data_.data() + static_cast<size_t>(channel) * static_cast<size_t>(numSamples_);
    }

    void clear() { std::fill(data_.begin(), data_.end(), 0.f); }

    void clear(int startSample, int numSamples) {
        for (int ch = 0; ch < numChannels_; ch++)
            std::fill_n(getWritePointer(ch) + startSample, numSamples, 0.f);
    }

    void applyGain(float gain) {
        for (auto& s : data_) s *= gain;
    }

    void applyGain(int channel, int startSample, int numSamples, float gain) {
        auto* p = getWritePointer(channel) + startSample;
        for (int i = 0; i < numSamples; i++) p[i] *= gain;
    }

    float getMagnitude(int channel, int startSample, int numSamples) const {
        const float* p = getReadPointer(channel) + startSample;
        float peak = 0.f;
        for (int i = 0; i < numSamples; i++) peak = std::max(peak, std::abs(p[i]));
        return peak;
    }

    float getMagnitude(int startSample, int numSamples) const {
        float peak = 0.f;
        for (int ch = 0; ch < numChannels_; ch++)
            peak = std::max(peak, getMagnitude(ch, startSample, numSamples));
        return peak;
    }

    void copyFrom(int destChannel, int destStartSample, const float* source, int numSamples) {
        std::memcpy(getWritePointer(destChannel) + destStartSample, source,
                     sizeof(float) * static_cast<size_t>(numSamples));
    }

private:
    void allocate(int numChannels, int numSamples) {
        numChannels_ = numChannels;
        numSamples_ = numSamples;
        data_.assign(static_cast<size_t>(numChannels) * static_cast<size_t>(numSamples), 0.f);
    }

    std::vector<float> data_;
    int numChannels_ = 0;
    int numSamples_ = 0;
};

} // namespace imagiro
