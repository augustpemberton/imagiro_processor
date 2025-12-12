//
// Created by August Pemberton on 24/06/2025.
//

#pragma once
#include <array>

#include "imagiro_processor/src/dsp/filter/CascadedBiquadFilter.h"
#include "juce_audio_basics/juce_audio_basics.h"

template <int Resolution = 1>
class MipmappedBuffer {
public:
    MipmappedBuffer() {
        for (auto& b : mipmapBuffers) {
            b = std::make_shared<juce::AudioSampleBuffer>(0, 0);
        }
    }

    MipmappedBuffer(const std::shared_ptr<juce::AudioSampleBuffer> &buffer, const float sampleRate) {
        for (auto& b : mipmapBuffers) {
            b = std::make_shared<juce::AudioSampleBuffer>(0, 0);
        }
        setBuffer(buffer, sampleRate);
    }

    void setBuffer(std::shared_ptr<juce::AudioSampleBuffer> buffer, const float sampleRate) {
        mipmapBuffers[0] = buffer;
        originalSampleRate = sampleRate;
        generateMipMaps();
    }

    std::shared_ptr<juce::AudioSampleBuffer> getBuffer(size_t factor) {
        return mipmapBuffers[std::min(factor, static_cast<size_t>(Resolution - 1))];
    }

    static int getResolution() { return Resolution; }
    double getSampleRate() const { return originalSampleRate; }

private:

    void generateMipMaps() {
        const auto originalBuffer = mipmapBuffers[0];

        for (auto i=1; i<Resolution; i++) {
            mipmapBuffers[i]->setSize(originalBuffer->getNumChannels(), originalBuffer->getNumSamples());

            CascadedBiquadFilter<8> filter;
            filter.setChannels(originalBuffer->getNumChannels());
            filter.setSampleRate(originalSampleRate);
            const auto factor = 1.f / (i + 1.f);
            filter.setCutoff(originalSampleRate * 0.5 * factor);

            for (auto c=0; c<originalBuffer->getNumChannels(); c++) {
                for (auto s=0; s<mipmapBuffers[i]->getNumSamples(); s++) {
                    const auto input = originalBuffer->getSample(c, s);
                    const auto filtered = filter.process(input, c);
                    mipmapBuffers[i]->setSample(c, s, filtered);
                }
            }
        }
    }

    std::array<std::shared_ptr<juce::AudioSampleBuffer>, Resolution> mipmapBuffers;
    double originalSampleRate {0};
};
