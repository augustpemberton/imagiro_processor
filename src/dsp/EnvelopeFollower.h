#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

class EnvelopeFollower {
public:

    EnvelopeFollower(float attackMS = 10, float releaseMS = 100) {
        setAttackMs(attackMS);
        setReleaseMs(releaseMS);
    }

    void setAttackMs(float attackMS) {
        this->attackMs = attackMS;
        recalculateCoefficients();
    }

    void setReleaseMs(float releaseMS) {
        this->releaseMs = releaseMS;
        recalculateCoefficients();
    }

    void setSampleRate(double sampleRate) {
        this->sampleRate = sampleRate;
        recalculateCoefficients();
    }

    void setEnvelope(float val) {
        envelope = val;
    }

    float pushSample(float sample, bool abs = true, float snap = -1) {
        float e = sample;
        if (abs) e = fabsf(e);
        envelope = (e<=envelope ? releaseCoeff : attackCoeff) * (envelope - e) + e;
        if (snap > 0) {
            if (fabsf(e-envelope) < snap) envelope = e;
        }
        return envelope;
    }

    float getEnvelope() const {
        return envelope;
    }

private:
    void recalculateCoefficients() {
        this->attackCoeff = pow( 0.01f, 1.0f / ( attackMs * (float)sampleRate * 0.001f ) );
        this->releaseCoeff = pow( 0.01f, 1.0f / ( releaseMs * (float)sampleRate * 0.001f ) );
    }

    std::atomic<float> attackCoeff{ 0 };
    std::atomic<float> releaseCoeff{ 0 };
    std::atomic<float> envelope { 0 };
    double sampleRate{ 0 };
    std::atomic<float> attackMs{ 0 };
    std::atomic<float> releaseMs{ 0 };
};