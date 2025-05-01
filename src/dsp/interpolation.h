//
// Created by August Pemberton on 28/07/2024.
//

#pragma once

#define INTERP_PRE_SAMPLES 2
#define INTERP_POST_SAMPLES 4

namespace imagiro {
    static float interpolateHQ(const juce::AudioSampleBuffer& b, int channel, float index) {
        jassert(index >= INTERP_PRE_SAMPLES && index < b.getNumSamples() - INTERP_POST_SAMPLES);

        const auto* in = b.getReadPointer(channel);
        const int floored = static_cast<int>(index);
        const float x = index - floored;

        const float ym1 = floored > 0 ? in[floored-1] : in[0];
        const float y0 = in[floored]    ;
        const float y1 = in[floored + 1];
        const float y2 = in[floored + 2];

        const float diff = y0 - y1;
        const float c1 = y1 - ym1;
        const float c3 = y2 - ym1 + 3.0f * diff;
        const float c2 = -(2.0f * diff + c1 + c3);

        return ((c3 * x + c2) * x + c1) * (x * 0.5f) + y0;
    }

    static float interpolateHQ6Point(const juce::AudioSampleBuffer& b, int channel, float index) {
        jassert(index >= 2 && index < b.getNumSamples() - 3);

        const auto* in = b.getReadPointer(channel);
        const int floored = static_cast<int>(index);
        const float x = index - floored;

        // Get 6 points centered on the position
        const float y0 = in[floored-2];
        const float y1 = in[floored-1];
        const float y2 = in[floored];
        const float y3 = in[floored+1];
        const float y4 = in[floored+2];
        const float y5 = in[floored+3];

        // 6-point kernel coefficients
        const float z = x - 0.5f;
        const float even1 = y2 + y3;
        const float odd1 = y3 - y2;
        const float even2 = y1 + y4;
        const float odd2 = y4 - y1;
        const float even3 = y0 + y5;
        const float odd3 = y5 - y0;

        return even1 + z * (odd1 + z * (even2 * 0.25f - even1 * 0.5f + even3 * 0.25f +
                                        z * (odd2 * 0.25f - odd1 + odd3 * 0.25f)));
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
