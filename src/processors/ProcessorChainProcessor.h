//
// Created by August Pemberton on 08/05/2025.
//

#pragma once
#include "diffuse-delay/DiffuseDelayProcessor.h"
#include "immer/array.hpp"
#include "juce_audio_processors/juce_audio_processors.h"

class ProcessorChainProcessor : public Processor, juce::Timer {
public:
    using ProcessorChain = std::list<std::shared_ptr<Processor> >;

    ProcessorChainProcessor()
        : Processor("") {
        startTimerHz(20);
    }

    /*
     * can be called from any thread!
     */
    void queueChain(const ProcessorChain &chain) {
        chainsToPrepare.enqueue(chain);
    }

    void prepareToPlay(double sampleRate, int blockSize) override {
        Processor::prepareToPlay(sampleRate, blockSize);
        for (const auto &processor: activeChain) {
            processor->prepareToPlay(sampleRate, blockSize);
        }
    }

    void process(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) override {
        ProcessorChain tempChain;

        while (preparedChains.try_dequeue(tempChain)) {
            activeChain = tempChain;
        }

        for (const auto &processor: activeChain) {
            renderProcessor(*processor, buffer, midiMessages);
        }
    }

private:
    ProcessorChain activeChain;
    moodycamel::ConcurrentQueue<ProcessorChain> chainsToPrepare{4};
    moodycamel::ReaderWriterQueue<ProcessorChain, 4> preparedChains;

    void timerCallback() override {
        ProcessorChain tempChain;
        while (chainsToPrepare.try_dequeue(tempChain)) {
            for (auto &processor: tempChain) {
                prepareProcessor(*processor);
            }

            preparedChains.enqueue(tempChain);
        }
    }

    void prepareProcessor(Processor &p) const {
        p.setPlayConfigDetails(
            getTotalNumInputChannels(),
            getTotalNumOutputChannels(),
            getSampleRate(),
            getBlockSize());
        p.prepareToPlay(getSampleRate(), getBlockSize());
    }

    void renderProcessor(Processor &p, juce::AudioSampleBuffer &buffer, juce::MidiBuffer &m) {
        p.setPlayHead(getPlayHead());
        p.processBlock(buffer, m);
    }

    void numChannelsChanged() override {
        for (const auto &processor: activeChain) {
            processor->setPlayConfigDetails(
                getTotalNumInputChannels(),
                getTotalNumOutputChannels(),
                getSampleRate(), getBlockSize());
        }
    }
};
