#pragma once
#include "juce_audio_basics/juce_audio_basics.h"
#include <string>
#include <memory>
#include <functional>

namespace imagiro {

class Transform {
public:
    virtual ~Transform() = default;

    // Apply the transform to a buffer (or create one in case of LoadTransform)
    // Returns true on success, false on error
    virtual bool process(juce::AudioSampleBuffer& buffer, double& sampleRate) const = 0;

    // Get error message if process() returned false
    virtual std::string getLastError() const { return "Unknown error"; }

    // Get unique hash for this transform
    virtual size_t getHash() const = 0;

    // Clone for polymorphic copying
    virtual std::unique_ptr<Transform> clone() const = 0;

    // For debugging/logging
    virtual std::string getDescription() const = 0;
};

} // namespace imagiro
