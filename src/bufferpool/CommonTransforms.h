#pragma once
#include <utility>

#include "Transform.h"
#include "imagiro_processor/src/dsp/filter/CascadedBiquadFilter.h"
#include "juce_dsp/juce_dsp.h"

class LoadTransform : public Transform {
public:
    LoadTransform(std::string path,
                  juce::AudioFormatManager& afm,
                  const int startSample = 0,
                  const int endSample = 0,
                  const int numChannels = 0)
        : filePath(std::move(path)),
          startSample(std::max(0, startSample)),
          endSample(std::max(0, endSample)),
          numChannels(std::max(0, numChannels)),
          afm(afm) {
    }

    bool process(juce::AudioSampleBuffer& buffer, double& sampleRate) const override {
        const auto file = juce::File(filePath);
        if (!file.existsAsFile()) {
            lastError = "File does not exist: " + filePath;
            return false;
        }

        std::unique_ptr<juce::AudioFormatReader> reader(afm.createReaderFor(file));
        if (!reader) {
            lastError = "Cannot create reader for file: " + filePath;
            return false;
        }

        // Determine range to load
        const auto totalSamples = reader->lengthInSamples;
        const auto start = std::min(startSample, static_cast<size_t>(totalSamples));
        const auto end = endSample == 0 ? totalSamples : std::min(static_cast<juce::int64>(endSample), totalSamples);
        const auto samplesToRead = end - start;

        if (samplesToRead <= 0) {
            lastError = "Invalid sample range";
            return false;
        }

        // Determine channels
        const auto channelsToRead = numChannels == 0
                                        ? reader->numChannels
                                        : std::min(numChannels, static_cast<int>(reader->numChannels));

        // Allocate buffer
        buffer.setSize(channelsToRead, static_cast<int>(samplesToRead));

        // Read audio
        if (!reader->read(buffer.getArrayOfWritePointers(),
                          channelsToRead,
                          static_cast<juce::int64>(start),
                          static_cast<int>(samplesToRead))) {
            lastError = "Failed to read audio data";
            return false;
        }

        sampleRate = reader->sampleRate;
        return true;
    }

    size_t getHash() const override {
        size_t h = std::hash<std::string>{}(filePath);
        h ^= std::hash<size_t>{}(startSample) << 1;
        h ^= std::hash<size_t>{}(endSample) << 2;
        h ^= std::hash<int>{}(numChannels) << 3;
        return h;
    }

    std::unique_ptr<Transform> clone() const override {
        return std::make_unique<LoadTransform>(*this);
    }

    std::string getDescription() const override {
        return "Load: " + filePath;
    }

    std::string getLastError() const override { return lastError; }

    const std::string& getFilePath() const { return filePath; }
    size_t getStartSample() const { return startSample; }
    size_t getEndSample() const { return endSample; }

private:
    std::string filePath;
    size_t startSample;
    size_t endSample;
    int numChannels;
    mutable std::string lastError;

    juce::AudioFormatManager& afm;
};

// Bandpass filter transform
class BandpassTransform : public Transform {
    float lowFreq;
    float highFreq;
    mutable std::string lastError;

public:
    BandpassTransform(float low, float high)
        : lowFreq(low), highFreq(high) {
    }

    bool process(juce::AudioSampleBuffer& buffer, double& sampleRate) const override {
        if (lowFreq <= 20 && highFreq >= 20000) return true;

        if (sampleRate <= 0) {
            lastError = "Invalid sample rate";
            return false;
        }

        CascadedBiquadFilter<4> lp;
        CascadedBiquadFilter<4> hp;
        lp.setFilterType(CascadedBiquadFilter<4>::LOWPASS);
        hp.setFilterType(CascadedBiquadFilter<4>::HIGHPASS);
        lp.setSampleRate(sampleRate);
        hp.setSampleRate(sampleRate);
        lp.setCutoff(highFreq);
        hp.setCutoff(lowFreq);
        lp.setChannels(buffer.getNumChannels());
        hp.setChannels(buffer.getNumChannels());

        for (auto c = 0; c < buffer.getNumChannels(); c++) {
            for (auto s = 0; s < buffer.getNumSamples(); s++) {
                auto v = buffer.getSample(c, s);
                v = lp.process(v, c);
                v = hp.process(v, c);
                buffer.setSample(c, s, v);
            }
        }
        return true;
    }

    size_t getHash() const override {
        size_t h = std::hash<float>{}(lowFreq);
        h ^= std::hash<float>{}(highFreq) << 1;
        return h;
    }

    std::unique_ptr<Transform> clone() const override {
        return std::make_unique<BandpassTransform>(lowFreq, highFreq);
    }

    std::string getDescription() const override {
        return "Bandpass: " + std::to_string(lowFreq) + "-" + std::to_string(highFreq) + "Hz";
    }

    std::string getLastError() const override { return lastError; }
};

// Normalize transform
class NormalizeTransform : public Transform {
    mutable std::string lastError;

public:
    bool process(juce::AudioSampleBuffer& buffer, double& sampleRate) const override {
        float maxMag = buffer.getMagnitude(0, buffer.getNumSamples());
        if (maxMag > 0.0f) {
            buffer.applyGain(1.0f / maxMag);
        }
        return true;
    }

    size_t getHash() const override {
        return std::hash<std::string>{}("normalize");
    }

    std::unique_ptr<Transform> clone() const override {
        return std::make_unique<NormalizeTransform>();
    }

    std::string getDescription() const override {
        return "Normalize";
    }

    std::string getLastError() const override { return lastError; }
};

// Gain transform
class GainTransform : public Transform {
    float gainDb;
    mutable std::string lastError;

public:
    GainTransform(float db) : gainDb(db) {
    }

    bool process(juce::AudioSampleBuffer& buffer, double& sampleRate) const override {
        buffer.applyGain(juce::Decibels::decibelsToGain(gainDb));
        return true;
    }

    size_t getHash() const override {
        return std::hash<float>{}(gainDb);
    }

    std::unique_ptr<Transform> clone() const override {
        return std::make_unique<GainTransform>(gainDb);
    }

    std::string getDescription() const override {
        return "Gain: " + std::to_string(gainDb) + "dB";
    }

    std::string getLastError() const override { return lastError; }
};
