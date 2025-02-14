//
// Created by August Pemberton on 20/01/2025.
//

#pragma once

class AudioTimer {
public:
    AudioTimer() = default;

    bool advance() {
        if (!active) return false;

        if (secondsSinceLastTick < 0) secondsSinceLastTick = rate;

        secondsSinceLastTick += secondsPerTick;

        bool ticking = false;
        if (secondsSinceLastTick > rate) {
            ticking = true;
            secondsSinceLastTick = fmod(secondsSinceLastTick, rate);
        }

        return ticking;
    }

    void setSampleRate(double sr) {
        sampleRate = sr;
        secondsPerTick = static_cast<float>(1.0 / sr);
    }

    void setRate(float seconds) {
        rate = seconds;
    }

    void setActive(bool _active) {
        active = _active;
        if (!this->active) {
            secondsSinceLastTick = -1;
        }
    }

private:
    bool active = true;
    double sampleRate = 0;
    float secondsPerTick = 0;

    float rate = 0.4f;

    float secondsSinceLastTick = -1;
};
