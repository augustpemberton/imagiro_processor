#pragma once

#include <juce_dsp/juce_dsp.h>

class AWeightingFilter
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        // A-weighting consists of:
        // - Two high-pass filters at 20.6 Hz
        // - Two high-pass filters at 107.7 Hz
        // - Two low-pass filters at 737.9 Hz
        // - Two low-pass filters at 12.2 kHz

        constexpr float f1 = 20.598997f;
        constexpr float f2 = 107.65265f;
        constexpr float f3 = 737.86223f;
        constexpr float f4 = 12194.217f;

        // Create cascaded filters
        highPass1.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(
            sampleRate, f1);
        highPass2.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(
            sampleRate, f1);
        highPass3.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(
            sampleRate, f2);
        highPass4.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(
            sampleRate, f2);

        lowPass1.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(
            sampleRate, f3);
        lowPass2.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(
            sampleRate, f3);
        lowPass3.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(
            sampleRate, f4);
        lowPass4.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(
            sampleRate, f4);

        // Apply normalization gain (A-weighting has +2dB at 1kHz)
        normalizationGain = 1.0f / 0.794328235f; // Approx 2dB boost

        reset();
    }

    void reset()
    {
        highPass1.reset();
        highPass2.reset();
        highPass3.reset();
        highPass4.reset();
        lowPass1.reset();
        lowPass2.reset();
        lowPass3.reset();
        lowPass4.reset();
    }

    float processSample(float sample)
    {
        // Cascade all filters
        sample = highPass1.processSample(sample);
        sample = highPass2.processSample(sample);
        sample = highPass3.processSample(sample);
        sample = highPass4.processSample(sample);
        sample = lowPass1.processSample(sample);
        sample = lowPass2.processSample(sample);
        sample = lowPass3.processSample(sample);
        sample = lowPass4.processSample(sample);

        return sample * normalizationGain;
    }

    void processBlock(juce::AudioBuffer<float>& buffer)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* data = buffer.getWritePointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                data[sample] = processSample(data[sample]);
            }
        }
    }

private:
    double sampleRate = 44100.0;
    float normalizationGain = 1.0f;

    juce::dsp::IIR::Filter<float> highPass1, highPass2, highPass3, highPass4;
    juce::dsp::IIR::Filter<float> lowPass1, lowPass2, lowPass3, lowPass4;
};