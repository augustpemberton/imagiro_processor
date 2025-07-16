#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include "AudioFFT.h"

class PitchDetector {
public:
    struct PitchCandidate {
        float frequency;
        float confidence;
        int startFrame;
        int duration;
    };

    struct Parameters {
        float mpmCutoff = 0.93f;
        float mpmSmallCutoff = 0.5f;
        float yinTolerance = 0.15f;
        float frequencyTolerance = 0.03f;
        bool useAdaptiveHopSize = true;      // Dynamically adjust hop size
        bool useHierarchicalSearch = false;   // Coarse-to-fine search
        bool runBothAlgorithms = false;      // Only run both if needed
        int maxBlocksToAnalyze = 10;         // Limit analysis for very long files
    };

    explicit PitchDetector(int blockSize = 4096)
        : blockSize(blockSize)
        , fftSize(2 * blockSize)
        , halfBlockSize(blockSize / 2) {
        fft.init(fftSize);

        // Pre-allocate reusable buffers
        nsdfBuffer.resize(halfBlockSize);
        yinBuffer.resize(halfBlockSize);
        fftReal.resize(audiofft::AudioFFT::ComplexSize(fftSize));
        fftImag.resize(audiofft::AudioFFT::ComplexSize(fftSize));
        fftInput.resize(fftSize);
        fftOutput.resize(fftSize);
    }

    void setParameters(const Parameters& params) {
        parameters = params;
    }

    // Optimized main detection function
    float detectPitchHz(const juce::AudioSampleBuffer& buffer, double sampleRate) {
        if (buffer.getNumSamples() < blockSize) return 0.0f;

        // Step 1: Quick pre-analysis to determine strategy
        auto strategy = determineAnalysisStrategy(buffer, sampleRate);

        // Step 2: Analyze using optimized approach
        std::vector<PitchCandidate> candidates;

        if (strategy.useHierarchical) {
            candidates = hierarchicalAnalysis(buffer, sampleRate, strategy);
        } else {
            candidates = standardAnalysis(buffer, sampleRate, strategy);
        }

        // Step 3: Find dominant pitch
        return findDominantPitch(candidates);
    }

private:
    const int blockSize;
    const int fftSize;
    const int halfBlockSize;
    audiofft::AudioFFT fft;
    Parameters parameters;

    // Pre-allocated buffers to avoid repeated allocations
    std::vector<float> nsdfBuffer;
    std::vector<float> yinBuffer;
    std::vector<float> fftReal;
    std::vector<float> fftImag;
    std::vector<float> fftInput;
    std::vector<float> fftOutput;

    struct AnalysisStrategy {
        bool useHierarchical;
        bool needsBothAlgorithms;
        int initialHopSize;
        int numBlocksToAnalyze;
    };

    // Determine optimal analysis strategy based on signal characteristics
    AnalysisStrategy determineAnalysisStrategy(const juce::AudioSampleBuffer& buffer, double sampleRate) {
        AnalysisStrategy strategy;

        // Quick energy analysis to determine signal characteristics
        const float* data = buffer.getReadPointer(0);
        const int numSamples = buffer.getNumSamples();

        // Calculate basic statistics
        float totalEnergy = 0.0f;
        float maxSample = 0.0f;
        for (int i = 0; i < std::min(numSamples, blockSize * 4); i += 16) {
            float sample = std::abs(data[i]);
            totalEnergy += sample * sample;
            maxSample = std::max(maxSample, sample);
        }

        // Normalize energy
        float avgEnergy = totalEnergy / (std::min(numSamples, blockSize * 4) / 16);

        // Determine strategy
        strategy.useHierarchical = parameters.useHierarchicalSearch && numSamples > blockSize * 8;
        strategy.needsBothAlgorithms = parameters.runBothAlgorithms;
        strategy.initialHopSize = parameters.useAdaptiveHopSize ?
            (avgEnergy > 0.3f ? blockSize / 2 : blockSize / 4) : blockSize / 4;
        strategy.numBlocksToAnalyze = std::min(
            parameters.maxBlocksToAnalyze,
            (numSamples / strategy.initialHopSize) + 1
        );

        return strategy;
    }

    // Hierarchical analysis: coarse to fine
    std::vector<PitchCandidate> hierarchicalAnalysis(
        const juce::AudioSampleBuffer& buffer,
        double sampleRate,
        const AnalysisStrategy& strategy) {

        std::vector<PitchCandidate> candidates;
        const float* data = buffer.getReadPointer(0);

        // Phase 1: Coarse analysis with large hop size
        int coarseHop = blockSize;
        std::vector<std::pair<int, float>> promisingRegions;

        for (int pos = 0; pos <= buffer.getNumSamples() - blockSize; pos += coarseHop) {
            // Quick MPM-only analysis
            float pitch = quickMPMAnalysis(data + pos, sampleRate);
            if (pitch > 20.0f) {
                promisingRegions.push_back({pos, pitch});
            }
        }

        // Phase 2: Fine analysis on promising regions
        for (const auto& region : promisingRegions) {
            int startPos = std::max(0, region.first - blockSize);
            int endPos = std::min(buffer.getNumSamples() - blockSize, region.first + blockSize * 2);

            // Analyze with fine hop size
            for (int pos = startPos; pos <= endPos; pos += strategy.initialHopSize) {
                float pitch = strategy.needsBothAlgorithms ?
                    detectPitchInBlockBoth(data + pos, sampleRate) :
                    detectPitchInBlockFast(data + pos, sampleRate);

                if (pitch > 20.0f) {
                    addOrExtendCandidate(candidates, pitch, pos, strategy.initialHopSize);
                }
            }
        }

        return candidates;
    }

    // Standard analysis for shorter files
    std::vector<PitchCandidate> standardAnalysis(
        const juce::AudioSampleBuffer& buffer,
        double sampleRate,
        const AnalysisStrategy& strategy) {

        std::vector<PitchCandidate> candidates;
        const float* data = buffer.getReadPointer(0);
        const int hopSize = strategy.initialHopSize;

        // Analyze evenly distributed blocks
        int step = std::max(1, (buffer.getNumSamples() - blockSize) / strategy.numBlocksToAnalyze);

        for (int pos = 0; pos <= buffer.getNumSamples() - blockSize; pos += step) {
            float pitch = strategy.needsBothAlgorithms ?
                detectPitchInBlockBoth(data + pos, sampleRate) :
                detectPitchInBlockFast(data + pos, sampleRate);

            if (pitch > 20.0f) {
                addOrExtendCandidate(candidates, pitch, pos, hopSize);
            }
        }

        return candidates;
    }

    // Quick MPM for coarse analysis
    float quickMPMAnalysis(const float* audioBuffer, double sampleRate) {
        // Use smaller window for speed
        const int quickSize = 1024;
        if (blockSize < quickSize) return detectPitchMPM(audioBuffer, sampleRate);

        // Quick NSDF calculation with decimation
        std::fill(nsdfBuffer.begin(), nsdfBuffer.begin() + quickSize/2, 0.0f);

        // Simple time-domain autocorrelation for speed
        for (int tau = 1; tau < quickSize/2; tau++) {
            float acf = 0.0f;
            float norm = 0.0f;

            // Decimate by 4 for speed
            for (int j = 0; j < quickSize - tau; j += 4) {
                acf += audioBuffer[j] * audioBuffer[j + tau];
                norm += audioBuffer[j] * audioBuffer[j] + audioBuffer[j + tau] * audioBuffer[j + tau];
            }

            nsdfBuffer[tau] = (norm > 0.0f) ? 2.0f * acf / norm : 0.0f;
        }

        // Quick peak finding
        float maxVal = 0.0f;
        int maxPos = 0;

        for (int i = 20; i < quickSize/2 - 1; i++) {
            if (nsdfBuffer[i] > nsdfBuffer[i-1] &&
                nsdfBuffer[i] > nsdfBuffer[i+1] &&
                nsdfBuffer[i] > maxVal) {
                maxVal = nsdfBuffer[i];
                maxPos = i;
            }
        }

        if (maxVal > parameters.mpmCutoff * 0.9f && maxPos > 0) {
            return static_cast<float>(sampleRate) / maxPos;
        }

        return -1.0f;
    }

    // Fast single-algorithm detection
    float detectPitchInBlockFast(const float* audioBuffer, double sampleRate) {
        return detectPitchMPM(audioBuffer, sampleRate);
    }

    // Full dual-algorithm detection (only when needed)
    float detectPitchInBlockBoth(const float* audioBuffer, double sampleRate) {
        auto [mpmPitch, mpmConfidence] = detectPitchMPMWithConfidence(audioBuffer, sampleRate);

        // Only run YIN if MPM is uncertain or detected low pitch
        if (mpmPitch < 20.0f || mpmConfidence < 0.8f || mpmPitch < 150.0f) {
            auto [yinPitch, yinConfidence] = detectPitchYINWithConfidence(audioBuffer, sampleRate);

            if (yinPitch > 20.0f && mpmPitch > 20.0f) {
                float relativeError = std::abs(mpmPitch - yinPitch) / std::min(mpmPitch, yinPitch);
                if (relativeError < 0.05f) {
                    return (mpmPitch * mpmConfidence + yinPitch * yinConfidence) /
                           (mpmConfidence + yinConfidence);
                }

                // For low frequencies or when MPM is uncertain, prefer YIN
                if (mpmPitch < 130.0f || mpmConfidence < 0.7f) {
                    return yinPitch;
                }
            } else if (yinPitch > 20.0f) {
                return yinPitch;
            }
        }

        return mpmPitch;
    }

    // Helper to add or extend candidates
    void addOrExtendCandidate(std::vector<PitchCandidate>& candidates,
                              float pitch, int position, int hopSize) {
        for (auto& candidate : candidates) {
            if (std::abs(candidate.frequency - pitch) / candidate.frequency < parameters.frequencyTolerance &&
                position - (candidate.startFrame + candidate.duration) <= hopSize * 2) {
                candidate.duration = position - candidate.startFrame + hopSize;
                candidate.frequency = (candidate.frequency + pitch) / 2.0f;
                return;
            }
        }

        candidates.push_back({pitch, 0.8f, position, hopSize});
    }

    // Find the pitch that appears most dominantly
    float findDominantPitch(const std::vector<PitchCandidate>& candidates) {
        if (candidates.empty()) return 0.0f;

        // Group similar pitches
        std::vector<std::pair<float, int>> pitchGroups;

        for (const auto& candidate : candidates) {
            bool found = false;
            for (auto& group : pitchGroups) {
                if (std::abs(group.first - candidate.frequency) / group.first < 0.05f) {
                    group.first = (group.first * group.second + candidate.frequency * candidate.duration) /
                                  (group.second + candidate.duration);
                    group.second += candidate.duration;
                    found = true;
                    break;
                }
            }

            if (!found) {
                pitchGroups.push_back({candidate.frequency, candidate.duration});
            }
        }

        // Return the pitch with the longest total duration
        auto dominant = std::max_element(pitchGroups.begin(), pitchGroups.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });

        return dominant->first;
    }

    // Include the optimized MPM and YIN implementations from before
    std::pair<float, float> detectPitchMPMWithConfidence(const float* audioBuffer, double sampleRate) {
        // Use pre-allocated buffers
        std::vector<float>& nsdf = nsdfBuffer;
        calculateNSDF(audioBuffer, nsdf);

        std::vector<int> maxPositions = findPeaks(nsdf);
        if (maxPositions.empty()) return {-1.0f, 0.0f};

        float highestAmplitude = 0.0f;
        for (int pos : maxPositions) {
            highestAmplitude = std::max(highestAmplitude, nsdf[pos]);
        }

        std::vector<std::pair<float, float>> estimates;
        for (int tau : maxPositions) {
            if (nsdf[tau] > parameters.mpmSmallCutoff) {
                auto estimate = parabolicInterpolation(nsdf, tau);
                estimates.push_back(estimate);
            }
        }

        if (estimates.empty()) return {-1.0f, 0.0f};

        float actualCutoff = parameters.mpmCutoff * highestAmplitude;
        for (const auto& estimate : estimates) {
            if (estimate.second >= actualCutoff) {
                float period = estimate.first;
                float pitch = static_cast<float>(sampleRate) / period;
                if (pitch > 80.0f) {
                    float confidence = estimate.second / highestAmplitude;
                    return {pitch, confidence};
                }
            }
        }

        return {-1.0f, 0.0f};
    }

    float detectPitchMPM(const float* audioBuffer, double sampleRate) {
        return detectPitchMPMWithConfidence(audioBuffer, sampleRate).first;
    }

    std::pair<float, float> detectPitchYINWithConfidence(const float* audioBuffer, double sampleRate) {
        std::vector<float>& yin = yinBuffer;

        // Calculate difference function
        for (int tau = 1; tau < halfBlockSize; tau++) {
            float sum = 0.0f;
            for (int j = 0; j < halfBlockSize; j++) {
                float delta = audioBuffer[j] - audioBuffer[j + tau];
                sum += delta * delta;
            }
            yin[tau] = sum;
        }

        // Cumulative mean normalization
        yin[0] = 1.0f;
        float runningSum = 0.0f;
        for (int tau = 1; tau < halfBlockSize; tau++) {
            runningSum += yin[tau];
            yin[tau] *= tau / runningSum;
        }

        // Find minimum
        float minValue = 1.0f;
        int minTau = -1;
        for (int tau = 2; tau < halfBlockSize - 1; tau++) {
            if (yin[tau] < parameters.yinTolerance && yin[tau] < yin[tau + 1]) {
                minValue = yin[tau];
                minTau = tau;
                break;
            }
        }

        if (minTau == -1) {
            auto minIt = std::min_element(yin.begin() + 1, yin.begin() + halfBlockSize);
            minTau = static_cast<int>(std::distance(yin.begin(), minIt));
            minValue = *minIt;
        }

        auto [betterTau, betterValue] = parabolicInterpolation(yin, minTau);
        float pitch = static_cast<float>(sampleRate) / betterTau;
        float confidence = 1.0f - std::min(betterValue, 1.0f);

        return {pitch, confidence};
    }

    // Optimized NSDF calculation using pre-allocated buffers
    void calculateNSDF(const float* audioBuffer, std::vector<float>& nsdf) {
        // Clear and prepare buffers
        std::fill(fftInput.begin(), fftInput.end(), 0.0f);
        std::copy(audioBuffer, audioBuffer + blockSize, fftInput.begin());

        // FFT
        fft.fft(fftInput.data(), fftReal.data(), fftImag.data());

        // Power spectrum
        for (size_t i = 0; i < fftReal.size(); i++) {
            float magnitude = fftReal[i] * fftReal[i] + fftImag[i] * fftImag[i];
            fftReal[i] = magnitude;
            fftImag[i] = 0.0f;
        }

        // IFFT
        fft.ifft(fftOutput.data(), fftReal.data(), fftImag.data());

        // Calculate normalization
        nsdf[0] = 1.0f;
        float sum0 = 0.0f;
        for (int i = 0; i < blockSize; i++) {
            sum0 += audioBuffer[i] * audioBuffer[i];
        }

        for (int tau = 1; tau < halfBlockSize; tau++) {
            float sumTau = 0.0f;
            for (int i = tau; i < blockSize; i++) {
                sumTau += audioBuffer[i] * audioBuffer[i];
            }

            float denominator = sum0 + sumTau;
            nsdf[tau] = (denominator > 0.0f) ? 2.0f * fftOutput[tau] / denominator : 0.0f;
            sum0 = sumTau;
        }
    }

    std::vector<int> findPeaks(const std::vector<float>& nsdf) {
        std::vector<int> peaks;
        int pos = 1;

        while (pos < halfBlockSize - 1 && nsdf[pos] > 0) pos++;
        while (pos < halfBlockSize - 1 && nsdf[pos] <= 0) pos++;

        while (pos < halfBlockSize - 1) {
            if (nsdf[pos] > nsdf[pos - 1] && nsdf[pos] >= nsdf[pos + 1]) {
                peaks.push_back(pos);
                while (pos < halfBlockSize - 1 && nsdf[pos] > 0) pos++;
            }
            pos++;
        }

        return peaks;
    }

    std::pair<float, float> parabolicInterpolation(const std::vector<float>& array, int x) {
        if (x <= 0 || x >= static_cast<int>(array.size()) - 1) {
            return {static_cast<float>(x), array[x]};
        }

        float s0 = array[x - 1];
        float s1 = array[x];
        float s2 = array[x + 1];

        float denominator = s0 - 2.0f * s1 + s2;
        if (std::abs(denominator) < 1e-10f) {
            return {static_cast<float>(x), s1};
        }

        float delta = 0.5f * (s0 - s2) / denominator;
        return {x + delta, s1 - 0.25f * (s0 - s2) * delta};
    }
};