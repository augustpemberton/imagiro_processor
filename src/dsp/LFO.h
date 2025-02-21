#pragma once

class LFO {
public:
    enum Shape {
        Sine,
        Triangle,
        RampDown,
        RampUp
    };

    LFO() : phase(0.0f), shape(Sine) {}

    void setFrequency(float freq) {
        frequency = freq;
        phaseInc = frequency / sampleRate;
    }

    void setSampleRate(float sr) {
        sampleRate = sr;
        phaseInc = frequency / sampleRate;
    }

    void setShape(Shape newShape) {
        shape = newShape;
    }

    void setPhase(float phase01) {
        phase = phase01;
    }

    float process(int numSamples = 1) {
        // Advance phase by multiple samples
        phase += phaseInc * numSamples;
        while (phase >= 1.0f) {
            phase -= 1.0f;
        }

        float output = 0.f;
        if (shape == Shape::Sine) {
            float p = phase < 0.5f ? phase : (1.f - phase);
            output = fastersin(p * 2 * 3.1415f) * (phase < 0.5f ? 1.f : -1.f);
        } else if (shape == Shape::Triangle) {
            if (phase < 0.5f) {
                output = 4.0f * phase - 1.0f;
            } else {
                output = 3.0f - 4.0f * phase;
            }
        } else if (shape == Shape::RampDown) {
            output = 1.0f - 2.0f * phase;
        } else if (shape == Shape::RampUp) {
            output = 2.0f * phase - 1.0f;
        }

        return output;
    }

private:
    float phase;
    float phaseInc;
    Shape shape;

    float sampleRate {48000};
    float frequency {2};
};