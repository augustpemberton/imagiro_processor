#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

template <typename T = float>
class EnvelopeFollower {
public:
    explicit EnvelopeFollower(float attackMS = 10, float releaseMS = 10, double sr = 0) {
        setAttackMs(attackMS);
        setReleaseMs(releaseMS);
        if (sr > 0) setSampleRate(sr);
    }

    void setAttackMs(float attackMS) {
        this->attackMs = attackMS;
        recalculateCoefficients();
    }

    void setReleaseMs(float releaseMS) {
        this->releaseMs = releaseMS;
        recalculateCoefficients();
    }

    void setSampleRate(double sr) {
        sampleRate = sr;
        recalculateCoefficients();
    }

    void setTargetValue(T target) {
        targetValue = target;
    }

    void reset() {
        envelope = 0;
    }

    void skip(int numSamples) {
        if (targetValue <= envelope) {
            envelope = targetValue + (envelope - targetValue) * static_cast<T>(std::pow(releaseCoeff, numSamples));
        } else {
            envelope = targetValue + (envelope - targetValue) * static_cast<T>(std::pow(attackCoeff, numSamples));
        }
    }

    T pushSample(float sample, T snap = -1) {
        setTargetValue(sample);
        skip(1);
        if (snap > 0 && std::abs(targetValue - envelope) < snap) {
            envelope = targetValue;
        }
        return envelope;
    }

    T getCurrentValue() const { return envelope; }
    T getTargetValue() const { return targetValue; }
    float getAttackMS() const { return attackMs; }
    float getReleaseMS() const { return releaseMs; }

private:
    void recalculateCoefficients() {
        attackCoeff = std::pow(0.01f, 1.0f / (attackMs * (float)sampleRate * 0.001f));
        releaseCoeff = std::pow(0.01f, 1.0f / (releaseMs * (float)sampleRate * 0.001f));
    }

    float attackCoeff{0};
    float releaseCoeff{0};
    T envelope{0};
    T targetValue{0};
    double sampleRate{0};
    float attackMs{0};
    float releaseMs{0};
};