//
// Created by August Pemberton on 15/07/2025.
//

#pragma once
#include <juce_dsp/juce_dsp.h>

namespace imagiro {
    inline std::unique_ptr<juce::AudioBuffer<float>> downsampleBuffer(const juce::AudioBuffer<float>& inputBuffer,
                                                                      const double downsampleFactor)
    {
        if (downsampleFactor <= 1.0 || inputBuffer.getNumSamples() == 0)
            return std::make_unique<juce::AudioBuffer<float>>(inputBuffer); // No downsampling needed

        const int inputNumSamples = inputBuffer.getNumSamples();
        const int numChannels = inputBuffer.getNumChannels();
        const int outputNumSamples = static_cast<int>(inputNumSamples / downsampleFactor);

        auto outputBuffer = std::make_unique<juce::AudioBuffer<float>>(numChannels, outputNumSamples);
        outputBuffer->clear();

        juce::LagrangeInterpolator interpolator;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* input = inputBuffer.getReadPointer(ch);
            float* output = outputBuffer->getWritePointer(ch);

            interpolator.reset();

            interpolator.process(downsampleFactor, input, output, outputNumSamples);
        }

        return outputBuffer;
    }
}
