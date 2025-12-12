//
// Created by August Pemberton on 12/12/2025.
//

#pragma once

#include <atomic>
#include <sigslot/sigslot.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "imagiro_util/util.h"

namespace imagiro {

class TransportState {
public:
    void update(const juce::AudioPlayHead* playhead, double sampleRate) {
        lastSampleRate_.store(sampleRate, std::memory_order_relaxed);

        if (!playhead) return;

        auto pos = playhead->getPosition();
        if (!pos) return;

        if (auto bpm = pos->getBpm()) {
            const auto newBpm = *bpm > 0.01 ? *bpm : defaultBpm_;
            const auto oldBpm = lastBpm_.exchange(newBpm, std::memory_order_relaxed);
            if (!almostEqual(newBpm, oldBpm)) {
                bpmChanged(newBpm);
            }
        }

        if (auto playing = pos->getIsPlaying()) {
            const bool oldPlaying = lastPlaying_.exchange(playing, std::memory_order_relaxed);
            if (playing != oldPlaying) {
                playStateChanged(playing);
            }
        }

        if (auto timeSig = pos->getTimeSignature()) {
            timeSigNumerator_.store(timeSig->numerator, std::memory_order_relaxed);
            timeSigDenominator_.store(timeSig->denominator, std::memory_order_relaxed);
        }

        if (auto ppq = pos->getPpqPosition()) {
            positionPpq_.store(*ppq, std::memory_order_relaxed);
        }
    }

    double bpm() const { return lastBpm_.load(std::memory_order_relaxed); }
    double sampleRate() const { return lastSampleRate_.load(std::memory_order_relaxed); }
    bool isPlaying() const { return lastPlaying_.load(std::memory_order_relaxed); }
    int timeSigNumerator() const { return timeSigNumerator_.load(std::memory_order_relaxed); }
    int timeSigDenominator() const { return timeSigDenominator_.load(std::memory_order_relaxed); }
    double positionPpq() const { return positionPpq_.load(std::memory_order_relaxed); }

    void setDefaultBpm(double bpm) { defaultBpm_ = bpm; }
    double defaultBpm() const { return defaultBpm_; }

    // Timing utilities
    double syncTimeSeconds(float proportionOfBeat) const {
        const float timeMult = 4.f * static_cast<float>(timeSigNumerator()) / static_cast<float>(timeSigDenominator());
        return 60.0 / bpm() * proportionOfBeat * timeMult;
    }

    double samplesPerBeat() const {
        return sampleRate() * 60.0 / bpm();
    }

    double noteLengthSamples(float proportionOfBeat) const {
        return proportionOfBeat * samplesPerBeat();
    }

    // Signals
    sigslot::signal<double> bpmChanged;
    sigslot::signal<bool> playStateChanged;
    sigslot::signal<double> sampleRateChanged;

private:
    std::atomic<double> lastBpm_{120.0};
    std::atomic<double> lastSampleRate_{48000.0};
    std::atomic<bool> lastPlaying_{false};
    std::atomic<int> timeSigNumerator_{4};
    std::atomic<int> timeSigDenominator_{4};
    std::atomic<double> positionPpq_{0.0};
    double defaultBpm_{120.0};
};

} // namespace imagiro