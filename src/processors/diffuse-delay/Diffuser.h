//
// Created by August Pemberton on 10/06/2024.
//

#pragma once
#include "./MixMatrix.h"
#include <random>
#include "imagiro_util/src/dsp/delay.h"

using namespace imagiro;

inline double randomInRange(double low, double high) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(low, high);
    return dist(gen);
}

template<int channels=8>
struct DiffusionStep {
    using Array = std::array<double, channels>;

    std::array<int, channels> delaySamples;
    std::array<signalsmith::delay::Delay<float>, channels> delays;

    std::array<bool, channels> flipPolarity;
    std::array<double, channels> channelOffsets;

    double sampleRate {0};
    juce::SmoothedValue<double> diffusionOffset {0};
    juce::SmoothedValue<double> diffusionSkew {0};
    double maxDelayTime {0};

    std::array<double, channels> lfoPhase {0};
    juce::SmoothedValue<double> lfoFreq {2};
    juce::SmoothedValue<double> lfoAmount {0.01};

    DiffusionStep(double maxDelayTime_ = 5.) {
        maxDelayTime = maxDelayTime_;

        for (int c = 0; c < channels; ++c) {
            flipPolarity[c] = rand() % 2;
            channelOffsets[c] = randomInRange(0., 1.);
            lfoPhase[c] = randomInRange(0., M_PI * 2);
            delays[c].reset(0);
        }
    }

    void configure(double sr) {
        sampleRate = sr;
        updateDiffusionLength();

        for (int c = 0; c < channels; ++c) {
            delays[c].resize(maxDelayTime * sampleRate + 1);
            delays[c].reset(0);
        }

        diffusionOffset.reset(sampleRate, 0.01);
        diffusionSkew.reset(sampleRate, 0.01);
        lfoAmount.reset(sampleRate, 0.01);
    }

    void setDiffusionLength(double offset, double skewTime) {
        offset = std::min(offset, maxDelayTime);
        diffusionSkew.setTargetValue(skewTime);
        diffusionOffset.setTargetValue(offset);

        if (sampleRate > 0) updateDiffusionLength();
    }

    void updateDiffusionLength() {
        double delaySamplesOffset = diffusionOffset.getNextValue() * sampleRate;
        double delaySamplesSkew = diffusionSkew.getNextValue() * sampleRate;
        for (int c = 0; c < channels; ++c) {
            double rangeLow = std::max(0., delaySamplesOffset - 0.5 * delaySamplesSkew);
            double rangeHigh = std::min(maxDelayTime * sampleRate, delaySamplesOffset + 0.5 * delaySamplesSkew);
            delaySamples[c] = lerp(rangeLow, rangeHigh, channelOffsets[c]);
        }
    }

    Array process(Array input) {
        if (diffusionSkew.isSmoothing() || diffusionOffset.isSmoothing()) updateDiffusionLength();

        // Delay
        Array delayed;
        const auto lfoFreqVal = lfoFreq.getNextValue();
        const auto lfoAmountVal = lfoAmount.getNextValue();
        for (int c = 0; c < channels; ++c) {
            auto freq = lerp(lfoFreqVal * 0.8, lfoFreqVal * 1.2, c/(double)channels);
            lfoPhase[c] += freq/ sampleRate;
            lfoPhase[c] = fmod(lfoPhase[c], M_PI * 2);
            auto lfoVal = fastsin(lfoPhase[c] - M_PI) * lfoAmountVal;

            auto delayPos = delaySamples[c] + lfoVal*sampleRate;
            delayPos = juce::jlimit(0., maxDelayTime * sampleRate, delayPos);

            delayed[c] = delays[c].read(delayPos);
            delays[c].write(input[c]);
        }

        // Mix with a Hadamard matrix
        Array mixed = delayed;
        // flip
        for (int c = 0; c < channels; ++c) {
            if (flipPolarity[c]) mixed[c] *= -1;
        }

        // mix
        Hadamard<double, channels>::inPlace(mixed.data());

        return mixed;
    }
};

template<int channels=8, int stepCount=4>
struct DiffuserHalfLengths {
    using Array = std::array<double, channels>;

    using Step = DiffusionStep<channels>;
    std::array<Step, stepCount> steps;

    double feedbackAmount;
    std::array<float, channels> feedback;

    double sampleRate {0};
    juce::SmoothedValue<double> lowpassCutoff {20000};
    juce::SmoothedValue<double> highpassCutoff {20};
    std::array<juce::IIRFilter, channels> lowpasses;
    std::array<juce::IIRFilter, channels> highpasses;

    DiffuserHalfLengths() {
        feedback.fill(0);
    }

    void setDiffusionLength(double offset, double skewTime) {
        for (auto &step : steps) {
            step.setDiffusionLength(offset/stepCount, skewTime/stepCount);
        }
    }

    void configure(double sr) {
        sampleRate = sr;
        for (auto &step : steps) step.configure(sampleRate);
        lowpassCutoff.reset(sampleRate, 0.05);
        highpassCutoff.reset(sampleRate, 0.05);
    }

    Array process(Array samples) {
        for (auto c=0; c<channels; c++) {
            samples[c] += feedback[c] * feedbackAmount;
        }

        for (auto &step : steps) {
            samples = step.process(samples);
        }

        auto lp = lowpassCutoff.getNextValue();
        auto hp = highpassCutoff.getNextValue();
        for (auto c=0; c<channels; c++) {
            lowpasses[c].setCoefficients(juce::IIRCoefficients::makeLowPass(sampleRate, lp));
            highpasses[c].setCoefficients(juce::IIRCoefficients::makeHighPass(sampleRate, hp));
            samples[c] = lowpasses[c].processSingleSampleRaw(samples[c]);
            samples[c] = highpasses[c].processSingleSampleRaw(samples[c]);
            feedback[c] = samples[c];
        }
        return samples;
    }

    void setFeedback(float amount) {
        feedbackAmount = amount;
    }

    void setHPCutoff(double cutoff) {
        this->highpassCutoff.setTargetValue(cutoff);
    }
    void setLPCutoff(double cutoff) {
        this->lowpassCutoff.setTargetValue(cutoff);
    }

    void setModAmount(double amount) {
        for (auto &step : steps) {
            step.lfoAmount.setTargetValue(amount * 0.005);
        }
    }

    void setModFreq(double freq) {
        for (auto &step : steps) {
            step.lfoFreq.setTargetValue(freq);
        }
    }
};

template<int channels=8, int diffusionSteps=4>
class Diffuser {
    using Array = std::array<double, channels>;
    DiffuserHalfLengths<channels, diffusionSteps> diffuser;

public:
    Diffuser(double diffusionOffset = 0.5, double diffusionSkew = 0.3)
    {
        diffuser.setDiffusionLength(diffusionOffset, diffusionSkew);
    }

    Array process(float* in, int numInputChannels) {
        Array input;
        // copy input to all channels
        for (auto c=numInputChannels; c<channels; c++) {
            input[c] = in[c%numInputChannels];
        }

        Array diffuse = diffuser.process(input);

        for (auto c=0; c<channels; c++) {
            const auto outputChan = c % numInputChannels;
            in[outputChan] = diffuse[c];
        }
    }

    void process(juce::AudioSampleBuffer& buffer) {
        for (auto s=0; s<buffer.getNumSamples(); s++) {
            Array input;
            for (auto c=0; c<channels; c++) {
                input[c] = buffer.getSample(c%buffer.getNumChannels(), s);
            }

            Array diffuse = diffuser.process(input);

            buffer.clear(s, 1);

            for (auto c=0; c<channels; c++) {
                const auto outputChan = c % buffer.getNumChannels();
                buffer.addSample(outputChan, s, diffuse[c]);
            }

        }
    }

    void setHPCutoff(double cutoff) {
        diffuser.setHPCutoff(cutoff);
    }

    void setLPCutoff(double cutoff) {
        diffuser.setLPCutoff(cutoff);
    }

    void prepare(double sampleRate, int blockSize) {
        diffuser.configure(sampleRate);
        for (auto& lowpass : diffuser.lowpasses) lowpass.reset();
        for (auto& highpass : diffuser.highpasses) highpass.reset();
    }

    void setDiffusionLength(double offset, double skewTime) {
        diffuser.setDiffusionLength(offset, skewTime);
    }

    void setFeedback(float feedback) {
        diffuser.setFeedback(feedback);
    }

    void setModAmount(double amount) {
        diffuser.setModAmount(amount);
    }

    void setModFreq(double freq) {
        diffuser.setModFreq(freq);
    }

};
