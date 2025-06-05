//
// Created by August Pemberton on 08/05/2025.
//

#pragma once
#include "diffuse-delay/DiffuseDelayProcessor.h"
#include "juce_audio_processors/juce_audio_processors.h"

class ProcessorChainProcessor : public Processor, juce::Timer {
public:
    using ProcessorChain = std::vector<std::shared_ptr<Processor>>;

    ProcessorChainProcessor(unsigned int numChannels = 2)
        : Processor("", ParameterLoader(), getProperties(numChannels)) {
        startTimerHz(20);
    }

    static BusesProperties getProperties(unsigned int numChannels = 2) {
        if (numChannels == 0) return BusesProperties();
        return BusesProperties()
                .withInput("Input", juce::AudioChannelSet::discreteChannels(numChannels), true)
                .withOutput("Output", juce::AudioChannelSet::discreteChannels(numChannels), true);
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
            prepareProcessor(*processor);
        }

        chainFadeGain.reset(sampleRate, 0.1);
    }

    void process(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) override {
        for (const auto &processor: activeChain) {
            renderProcessor(*processor, buffer, midiMessages);
        }

        chainFadeGain.applyGain(buffer, buffer.getNumSamples());

        if (fadingOut && chainFadeGain.getTargetValue() > 0.f) chainFadeGain.setTargetValue(0.f);

        // if fade is finished, swap out chains
        if (fadingOut && !chainFadeGain.isSmoothing() && chainFadeGain.getTargetValue() == 0.f) {
            chainsToDeallocate.enqueue(activeChain);
            while (preparedChains.try_dequeue(activeChain)) { }
            chainFadeGain.setTargetValue(1.f);
            fadingOut = false;
        }
    }

private:
    ProcessorChain activeChain;
    std::optional<ProcessorChain> oldChain;

    std::atomic<bool> fadingOut { false };
    juce::SmoothedValue<float> chainFadeGain {1};

    moodycamel::ConcurrentQueue<ProcessorChain> chainsToPrepare{4};
    moodycamel::ReaderWriterQueue<ProcessorChain> preparedChains {4};

    moodycamel::ReaderWriterQueue<ProcessorChain> chainsToDeallocate {4};

    void timerCallback() override {
        ProcessorChain tempChain;
        while (chainsToPrepare.try_dequeue(tempChain)) {
            for (auto &processor: tempChain) {
                // only prepare processor if it's not already in the current chain
                // otherwise it's already prepared! and might click during fadeout
                auto instanceInCurrentChain = std::ranges::find(activeChain, processor);
                if (instanceInCurrentChain == activeChain.end()) {
                    prepareProcessor(*processor);
                }
            }

            fadingOut = true;
            preparedChains.enqueue(tempChain);
        }

        // take deallocate chains off the fifo, so they deallocate here once they go out of scope
        while (chainsToDeallocate.try_dequeue(tempChain)) {
        }
    }

    void prepareProcessor(Processor &p) const {
        const auto numChannels = std::max(getTotalNumInputChannels(), getTotalNumOutputChannels());
        p.setPlayConfigDetails(
            numChannels,
            numChannels,
            lastSampleRate,
            getBlockSize());
        p.prepareToPlay(lastSampleRate, getBlockSize());
    }

    void renderProcessor(Processor &p, juce::AudioSampleBuffer &buffer, juce::MidiBuffer &m) const {
        p.setPlayHead(getPlayHead());
        p.processBlock(buffer, m);
    }

    void numChannelsChanged() override {
        for (const auto &processor: activeChain) {
            prepareProcessor(*processor);
        }
    }
};
