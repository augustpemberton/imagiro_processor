//
// Created by August Pemberton on 28/07/2024.
//

#pragma once

#define INTERP_PRE_SAMPLES 2
#define INTERP_POST_SAMPLES 4
#include "imagiro_util/util.h"
#include "juce_audio_basics/juce_audio_basics.h"

namespace imagiro {
    static float interp_linear(const juce::AudioSampleBuffer& b, int channel, double index) {
        jassert(index >= INTERP_PRE_SAMPLES && index < b.getNumSamples() - INTERP_POST_SAMPLES);

        const auto* in = b.getReadPointer(channel);
        const int floored = static_cast<int>(index);
        const auto x = index - floored;

        const auto y0 = in[floored];
        const auto y1 = in[floored + 1];

        return y0 + (y1 - y0) * x;
    }

    /*
     * 4-point, 3rd order for 2x oversampled audio
     */
    static float interp4p3o_2x(const juce::AudioSampleBuffer& b, int channel, double index) {
        jassert(index >= INTERP_PRE_SAMPLES && index < b.getNumSamples() - INTERP_POST_SAMPLES);

        const auto* in = b.getReadPointer(channel);
        const int floored = static_cast<int>(index);
        const auto x = index - floored;

        const auto ym1 = floored > 0 ? in[floored-1] : in[0];
        const auto y0 = in[floored];
        const auto y1 = in[floored + 1];
        const auto y2 = in[floored + 2];

        float z = x - 1/2.0;
        float even1 = y1+y0, odd1 = y1-y0;
        float even2 = y2+ym1, odd2 = y2-ym1;
        float c0 = even1*0.45868970870461956 + even2*0.04131401926395584;
        float c1 = odd1*0.48068024766578432 + odd2*0.17577925564495955;
        float c2 = even1*-0.246185007019907091 + even2*0.24614027139700284;
        float c3 = odd1*-0.36030925263849456 + odd2*0.10174985775982505;
        return ((c3*z+c2)*z+c1)*z+c0;
    }

    static float interp4p3o_2x(const float* in, double index) {
        // jassert(index >= INTERP_PRE_SAMPLES && index < b.getNumSamples() - INTERP_POST_SAMPLES);

        // const auto* in = b.getReadPointer(channel);
        const int floored = static_cast<int>(index);
        const auto x = index - floored;

        const auto ym1 = floored > 0 ? in[floored-1] : in[0];
        const auto y0 = in[floored];
        const auto y1 = in[floored + 1];
        const auto y2 = in[floored + 2];

        float z = x - 1/2.0;
        float even1 = y1+y0, odd1 = y1-y0;
        float even2 = y2+ym1, odd2 = y2-ym1;
        float c0 = even1*0.45868970870461956 + even2*0.04131401926395584;
        float c1 = odd1*0.48068024766578432 + odd2*0.17577925564495955;
        float c2 = even1*-0.246185007019907091 + even2*0.24614027139700284;
        float c3 = odd1*-0.36030925263849456 + odd2*0.10174985775982505;
        return ((c3*z+c2)*z+c1)*z+c0;
    }

    /*
     * 4-point, 3rd order for 4x oversampled audio
     */
    static float interp4p3o_4x(const juce::AudioSampleBuffer& b, int channel, double index) {
        jassert(index >= INTERP_PRE_SAMPLES && index < b.getNumSamples() - INTERP_POST_SAMPLES);

        const auto* in = b.getReadPointer(channel);
        const int floored = static_cast<int>(index);
        const auto x = index - floored;

        const auto ym1 = floored > 0 ? in[floored-1] : in[0];
        const auto y0 = in[floored];
        const auto y1 = in[floored + 1];
        const auto y2 = in[floored + 2];

        float z = x - 1/2.0;
        float even1 = y1+y0, odd1 = y1-y0;
        float even2 = y2+ym1, odd2 = y2-ym1;
        float c0 = even1*0.46209345013918979 + even2*0.03790693583186333;
        float c1 = odd1*0.51344507801315964 + odd2*0.16261507145522014;
        float c2 = even1*-0.248540332990294211 + even2*0.24853570133765701;
        float c3 = odd1*-0.42912649274763925 + odd2*0.13963062613760227;
        return ((c3*z+c2)*z+c1)*z+c0;
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

    static juce::AudioSampleBuffer repitchBuffer(const juce::AudioSampleBuffer& input, float pitchShiftSemitones) {         // Calculate the pitch shift factor
        float pitchShiftFactor = std::pow(2.0f, pitchShiftSemitones / 12.0f);

        int numChannels = input.getNumChannels();
        int inputLength = input.getNumSamples();
        int outputLength = std::round(inputLength / pitchShiftFactor);

        juce::AudioSampleBuffer output(numChannels, outputLength);

        if (almostEqual(pitchShiftSemitones, 0.f)) return input;

        for (int channel = 0; channel < numChannels; ++channel) {
            for (int outputSample = 0; outputSample < outputLength; ++outputSample) {
                float inputIndex = outputSample * pitchShiftFactor;

                // Use the interpolateHQ function for high-quality interpolation
                float interpolatedSample = imagiro::interp4p3o_2x(const_cast<juce::AudioSampleBuffer&>(input), channel, inputIndex);

                output.setSample(channel, outputSample, interpolatedSample);
            }
        }

        return output;
    }

};
