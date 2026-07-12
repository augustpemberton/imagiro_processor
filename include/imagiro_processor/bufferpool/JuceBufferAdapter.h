#pragma once
#include "AudioBuffer.h"
#include "juce_audio_basics/juce_audio_basics.h"
#include <cstring>

namespace imagiro {

// Non-owning juce::AudioSampleBuffer aliasing the storage of an imagiro::AudioBuffer.
// juce::AudioSampleBuffer's channel-pointer-array constructor copies the pointers into
// its own preallocated storage (for numChannels below its internal preallocatedChannelSpace
// size of 32) during construction, so the returned buffer is safe to use after this
// function returns even though the local pointer array does not outlive the call.
inline juce::AudioSampleBuffer toJuceView(imagiro::AudioBuffer& b) {
    float* channelPtrs[32];
    const int numChannels = b.getNumChannels();
    jassert(numChannels < 32);
    for (int ch = 0; ch < numChannels; ch++)
        channelPtrs[ch] = b.getWritePointer(ch);
    return juce::AudioSampleBuffer(channelPtrs, numChannels, b.getNumSamples());
}

inline void copyToJuce(const imagiro::AudioBuffer& src, juce::AudioSampleBuffer& dst) {
    dst.setSize(src.getNumChannels(), src.getNumSamples(), false, false, true);
    for (int ch = 0; ch < src.getNumChannels(); ch++)
        dst.copyFrom(ch, 0, src.getReadPointer(ch), src.getNumSamples());
}

inline void copyFromJuce(const juce::AudioSampleBuffer& src, imagiro::AudioBuffer& dst) {
    dst.setSize(src.getNumChannels(), src.getNumSamples());
    for (int ch = 0; ch < src.getNumChannels(); ch++)
        std::memcpy(dst.getWritePointer(ch), src.getReadPointer(ch),
                     sizeof(float) * static_cast<size_t>(src.getNumSamples()));
}

} // namespace imagiro
