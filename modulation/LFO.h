//
// Created by August Pemberton on 22/09/2022.
//

#pragma once
#include "Modulator.h"
#include <juce_dsp/juce_dsp.h>
#include <juce_dsp/juce_dsp.h>

namespace imagiro {
    class LFO {
    public:
        LFO(float freq = 1, float amplitude = 1, bool fast = false)
                :   fast(fast),
                    frequency(freq),
                    amplitude(amplitude)
        {}

        void setFrequency (float f) { this->frequency = f; }
        void setAmplitude (float amp) { this->amplitude = amp; }
        void setOffset (float degrees) { this->offsetDeg = degrees; }
        void setPhase (float phase01) { this->t = phase01; }

        [[nodiscard]] float getValue() const {
            auto phase01 = (t + offsetDeg / 360.f);
            phase01 -= (int)phase01; // same as phase01 %= 1
            /*
            auto v = amplitude *
                    (fast   ? sinApprox(phase01 * juce::MathConstants<float>::twoPi)
                            : sin(phase01 * juce::MathConstants<float>::twoPi));
                            */
            auto v = amplitude * sinApprox(phase01 * 3.14156f * 2.f);
            return (v + 1) / 2;
        }

        void advance(int samples) {
            auto s = getSamplesPerPeriod();
            if (s == 0) return;
            t += (float)samples / s;
            t = t - (int)t;
        }

        void setSampleRate(double sr) { sampleRate = sr; }

        //void setFast(bool shouldUseFast) { fast = shouldUseFast; }

    private:
        float getSamplesPerPeriod() { return (1.f / frequency) * (float)sampleRate; }

        juce::dsp::LookupTableTransform<float> sinApprox {
                [] (float x) { return (float)std::sin(x); },
                -6.5f, 6.5f, 200};

        const bool fast {false};

        float t {0};
        float offsetDeg {0};

        float frequency;
        float amplitude;
        double sampleRate {44100};
    };
}