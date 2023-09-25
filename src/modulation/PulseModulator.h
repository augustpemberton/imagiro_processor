//
// Created by August Pemberton on 23/09/2022.
//

#pragma once

namespace imagiro {
    class PulseModulator : public Modulator {
    public:
        PulseModulator(Parameter *rate, Parameter *depth, Parameter *damp, bool useHistory)
                : Modulator(Modulator::Type::Pulse, true, useHistory),
                  rate(rate),
                  depth(depth),
                  damp(damp) {
        }

        float getValue() override {
            auto ratio = (double) wobbleCounter / wobbleTime;
            if (ratio >= 1) return 0.5f;
            auto x = ratio * juce::MathConstants<double>::twoPi;

            auto v = static_cast<float>(juce::dsp::FastMathApproximations::sin(
                    x + juce::dsp::FastMathApproximations::sin(x) * wobbleSkew));

            v *= 0.5f * wobbleDepth;
            return v + 0.5f;
        }

        void prepareParameters() override {
            rateVal = rate->getModVal();
            depthVal = depth->getModVal();
            dampVal = damp->getModVal();
        }

        void advanceSample(juce::AudioSampleBuffer* b, int sampleIndex) override {

            auto max = wobbleTimeMaxRange.convertFrom0to1(1-rateVal);

            auto nextWobbleTimeSamples = wobbleTime + nextWobbleTime * max * sampleRate;

            if (nextWobbleCounter >= nextWobbleTimeSamples) {
                auto l = wobbleLengthRange.convertFrom0to1(dampVal);
                std::normal_distribution<float> lengthDistribution (l, l/5);
                auto wobbleLength_r = lengthDistribution(generator);

                nextWobbleTime = generateTimeToNextWobble() / max;
                nextWobbleCounter = 0;

                auto d = wobbleDepthRange.convertFrom0to1(depthVal);
                std::normal_distribution<float> depthDistribution (d, d/5);
                auto wobbleDepth_r = wobbleDepthRange.getRange().clipValue(depthDistribution(generator));
                startWobble(wobbleDepth_r, wobbleLength_r);
            }

            nextWobbleCounter ++;

            if (wobbleCounter > wobbleTime) wobbling = false;
            else wobbleCounter++;
        }

        void startWobble(float depth, float length) {
            if (wobbling) {
                DBG("already wobbling");
                return;
            }

            // Start wobble
            wobbleSkew = juce::Random::getSystemRandom().nextFloat() * 2 - 1;
            wobbleDepth = depth;

            wobbleTime = length * sampleRate;
            wobbleCounter = 0;

            wobbling = true;
        }

    private:
        float rateVal;
        float depthVal;
        float dampVal;

        Parameter* rate;
        Parameter* depth;
        Parameter* damp;

        std::default_random_engine generator;
        juce::Random noise;
        float nextWobbleCounter {0};
        float nextWobbleTime {0};

        float generateTimeToNextWobble() {
            auto min = wobbleTimeMinRange.convertFrom0to1(1-rate->getModVal());
            auto max = wobbleTimeMaxRange.convertFrom0to1(1-rate->getModVal());

            return juce::Random::getSystemRandom().nextFloat() * (max - min) + min;
        }

        juce::NormalisableRange<float> wobbleTimeMinRange {0.01f, 5.f, 0.f, 0.25f};
        juce::NormalisableRange<float> wobbleTimeMaxRange {0.07f, 8.f, 0.f, 0.3f};
        juce::NormalisableRange<float> wobbleLengthRange {0.04f, 1.f, 0.f, 0.8f};
        juce::NormalisableRange<float> wobbleDepthRange {0.f, 1.f};

        int wobbleCounter{ 0 };
        int wobbleTime{ 0 };

        float wobbleSkew {0};
        float wobbleDepth{ 0 };

        bool wobbling { false };
    };
}


