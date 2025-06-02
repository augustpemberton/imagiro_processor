//
// Created by August Pemberton on 30/05/2025.
//

#pragma once
#include <string>

class EnvelopeFollowerSource {
public:
    explicit EnvelopeFollowerSource(const std::string &_name) : name(_name)
    {
    }

    void update(juce::AudioSampleBuffer& b) {
        const auto multiplier = 1.f / b.getNumChannels();

        // skip the clear op by copying first channel instead of adding
        juce::FloatVectorOperations::copyWithMultiply(
            buffer.getWritePointer(0),
            b.getReadPointer(0),
            multiplier,
            b.getNumSamples());

        for (auto c=1; c<b.getNumChannels(); c++) {
            juce::FloatVectorOperations::addWithMultiply(
                buffer.getWritePointer(0),
                b.getReadPointer(c),
                multiplier,
                b.getNumSamples());
        }
    }

    juce::AudioSampleBuffer& getBuffer() { return buffer; }

    void prepare(int maxSamplesPerBlock) {
        buffer.setSize(1, maxSamplesPerBlock);
    }

    const std::string& getName() const { return name; }

private:
    juce::AudioSampleBuffer buffer;
    std::string name;
};
