//
// Created by August Pemberton on 28/07/2024.
//

#pragma once


namespace imagiro {
    static float interpolateHQ(juce::AudioSampleBuffer& b, int channel, float index) {

        auto in = b.getReadPointer(channel);

        int floored = ifloor(index);
        auto x = index - floored;

        auto ym1 = floored > 0 ? in[floored-1] : 0.f;
        auto y0 = in[floored];
        auto y1 = in[std::min(b.getNumSamples()-1, floored+1)];
        auto y2 = in[std::min(b.getNumSamples()-1, floored+2)];

        float diff = y0 - y1;
        float c1 = y1 - ym1;
        float c3 = y2 - ym1 + 3 * diff;
        float c2 = -(2 * diff + c1 + c3);
        return 0.5f * ((c3 * x + c2) * x + c1) * x + y0;
    }

    static juce::AudioSampleBuffer repitchBuffer(const juce::AudioSampleBuffer& input, float pitchShiftSemitones) {
        // Calculate the pitch shift factor
        float pitchShiftFactor = std::pow(2.0f, pitchShiftSemitones / 12.0f);

        int numChannels = input.getNumChannels();
        int inputLength = input.getNumSamples();
        int outputLength = std::round(inputLength / pitchShiftFactor);

        juce::AudioSampleBuffer output(numChannels, outputLength);

        for (int channel = 0; channel < numChannels; ++channel) {
            for (int outputSample = 0; outputSample < outputLength; ++outputSample) {
                float inputIndex = outputSample * pitchShiftFactor;

                // Use the interpolateHQ function for high-quality interpolation
                float interpolatedSample = imagiro::interpolateHQ(const_cast<juce::AudioSampleBuffer&>(input), channel, inputIndex);

                output.setSample(channel, outputSample, interpolatedSample);
            }
        }

        return output;
    }

};
