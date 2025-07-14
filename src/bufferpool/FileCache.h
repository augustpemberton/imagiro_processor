//
// Created by August Pemberton on 17/05/2024.
//

#pragma once

struct FileCacheEntry {
    std::unique_ptr<juce::AudioFormatReader> reader;
    juce::AudioSampleBuffer buffer;
    juce::AudioSampleBuffer bufferBandpassed;
    double sampleRate;
    float maxSample;
};

struct FileCacheKey {
    juce::String path;
    juce::int64 sourceStartSample;
    juce::int64 sourceEndSample;

    bool operator== (const FileCacheKey& other) const {
        return this->path == other.path &&
               this->sourceStartSample == other.sourceStartSample &&
               this->sourceEndSample == other.sourceEndSample;
    }
};

namespace std {
    template<>
    struct hash<FileCacheKey> {
        std::size_t operator()(const FileCacheKey& k) const {
            // Combine the hash of all members
            std::size_t h1 = std::hash<juce::String>{}(k.path);
            std::size_t h2 = std::hash<int>{}(k.sourceStartSample);
            std::size_t h3 = std::hash<int>{}(k.sourceEndSample);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}

class FileCache {
public:

    FileCache() {
        audioFormatManager.registerBasicFormats();
    }

    FileCacheEntry* get(const juce::String& filepath) {
        auto file = juce::File(filepath);
        auto r = std::unique_ptr<juce::AudioFormatReader> (audioFormatManager.createReaderFor(file));
        if (!r) return nullptr;
        auto endSample = r->lengthInSamples;

        return get(FileCacheKey{file.getFullPathName(), 0, endSample});
    }

    FileCacheEntry* get(const FileCacheKey& key) {
        auto file = juce::File(key.path);
        auto startSample = key.sourceStartSample;
        auto endSample = key.sourceEndSample;
        auto r = std::unique_ptr<juce::AudioFormatReader> (audioFormatManager.createReaderFor(file));
        if (!r) return nullptr;
        if (endSample == 0) endSample = r->lengthInSamples;

        auto b = cache.find({key.path, startSample, endSample});
        if (b != cache.end()) return &b->second;

        auto lengthSamples = endSample - startSample;

        auto bufferPair = cache.insert({
            FileCacheKey{key.path, key.sourceStartSample, endSample},
            FileCacheEntry{std::move(r), juce::AudioSampleBuffer(), juce::AudioSampleBuffer(), 0, 0}
        }).first;

        auto& entry = bufferPair->second;

        if (!entry.reader) return &entry;

        entry.sampleRate = entry.reader->sampleRate;

        // TODO we are only caching channels 1 and 2 for performance, add option to cache all channels
        auto numChannels = std::min(2, (int)entry.reader->numChannels);
        entry.buffer.setSize(numChannels, lengthSamples, false, true);
        entry.reader->read(&entry.buffer, 0, lengthSamples, startSample, true, true);

        entry.bufferBandpassed.makeCopyOf(entry.buffer);
        processFilters(entry);

        entry.maxSample = entry.buffer.getMagnitude(0, 0, entry.buffer.getNumSamples());
        return &entry;
    }

    void remove(const juce::String& filepath) {
        for (auto it = cache.begin(); it != cache.end(); ) {
            if (it->first.path == filepath) {
                it = cache.erase(it);
            } else {
                ++it;
            }
        }
    }

    void remove(const FileCacheKey& key) {
        cache.erase(key);
    }

    static void processFilters(FileCacheEntry& entry, float hpCutoff = 80, float lpCutoff = 3000) {
        for (auto channel=0; channel<entry.bufferBandpassed.getNumChannels(); channel++) {
            entry.bufferBandpassed.copyFrom(channel, 0,
                                            entry.buffer.getReadPointer(channel),
                                            entry.buffer.getNumSamples());

            juce::IIRFilter lowpassFilter;
            lowpassFilter.reset();
            lowpassFilter.setCoefficients(juce::IIRCoefficients::makeLowPass(entry.reader->sampleRate, lpCutoff));
            lowpassFilter.processSamples(entry.bufferBandpassed.getWritePointer(channel),
                                          entry.bufferBandpassed.getNumSamples());
            juce::IIRFilter highpassFilter;
            highpassFilter.reset();
            highpassFilter.setCoefficients(juce::IIRCoefficients::makeHighPass(entry.reader->sampleRate, hpCutoff));
            highpassFilter.processSamples(entry.bufferBandpassed.getWritePointer(channel),
                                          entry.bufferBandpassed.getNumSamples());
        }
    }

private:
    juce::AudioFormatManager audioFormatManager{};
    std::unordered_map<FileCacheKey, FileCacheEntry> cache{};
};
