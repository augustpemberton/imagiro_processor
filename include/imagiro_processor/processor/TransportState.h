//
// Created by August Pemberton on 12/12/2025.
//

#pragma once

#include <atomic>
#include <optional>
#include <sigslot/sigslot.h>

#include "imagiro_util/util-core.h"

namespace imagiro {

struct TransportInfo {
    std::optional<double> bpm;
    std::optional<bool> isPlaying;
    std::optional<int> timeSigNumerator;
    std::optional<int> timeSigDenominator;
    std::optional<double> ppqPosition;
};

class TransportState {
public:
    void update(const TransportInfo& info, double sampleRate) {
        lastSampleRate_.store(sampleRate, std::memory_order_relaxed);

        if (info.bpm) {
            const auto newBpm = *info.bpm > 0.01 ? *info.bpm : defaultBpm_;
            const auto oldBpm = lastBpm_.exchange(newBpm, std::memory_order_relaxed);
            if (!almostEqual(newBpm, oldBpm)) {
                bpmChanged(newBpm);
            }
        }

        if (info.isPlaying) {
            const bool playing = *info.isPlaying;
            const bool oldPlaying = lastPlaying_.exchange(playing, std::memory_order_relaxed);
            if (playing != oldPlaying) {
                playStateChanged(playing);
            }
        }

        if (info.timeSigNumerator) {
            timeSigNumerator_.store(*info.timeSigNumerator, std::memory_order_relaxed);
        }
        if (info.timeSigDenominator) {
            timeSigDenominator_.store(*info.timeSigDenominator, std::memory_order_relaxed);
        }

        if (info.ppqPosition) {
            positionPpq_.store(*info.ppqPosition, std::memory_order_relaxed);
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