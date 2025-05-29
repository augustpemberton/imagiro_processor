//
// Created by August Pemberton on 08/05/2025.
//

#pragma once
#include "diffuse-delay/DiffuseDelayProcessor.h"
#include "juce_audio_processors/juce_audio_processors.h"

class ProcessorChainProcessor : public Processor, juce::Timer {
public:
    using ProcessorChain = std::vector<std::shared_ptr<Processor> >;

    ProcessorChainProcessor()
        : Processor("", ParameterLoader(), getDefaultProperties()) {
        startTimerHz(20);
    }

    BusesProperties getDefaultProperties() {
        return BusesProperties()
                .withInput("Input", juce::AudioChannelSet::stereo(), true)
                .withOutput("Output", juce::AudioChannelSet::stereo(), true);
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
        chainFadePerSample = static_cast<float>(1 / (chainFadeTime * getSampleRate()));
        dryCopy.setSize(getTotalNumInputChannels(), blockSize);
    }

    void process(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) override {
        bool newChainAvailable = false;
        ProcessorChain newChain;
        while (preparedChains.try_dequeue(newChain)) { newChainAvailable = true; }

        if (newChainAvailable) {
            oldChain = activeChain;
            activeChain = newChain;
            chainFadeRatio = 1;
        }

        if (oldChain.has_value()) {
            // copy dry buffer
            for (auto c = 0; c < buffer.getNumChannels(); c++) {
                dryCopy.copyFrom(c, 0,
                                 buffer.getReadPointer(c), buffer.getNumSamples());
            }
        }

        for (const auto &processor: activeChain) {
            renderProcessor(*processor, buffer, midiMessages);
        }

        if (oldChain.has_value()) {

            // render old chain
            for (const auto &processor: *oldChain) {
                renderProcessor(*processor, dryCopy, midiMessages);
            }

            // mix old chain with new chain
            auto startRatio = chainFadeRatio;
            auto fadeThisBlock = chainFadePerSample * buffer.getNumSamples();
            chainFadeRatio = std::max(0.f, chainFadeRatio - fadeThisBlock);

            for (auto c = 0; c < buffer.getNumChannels(); c++) {
                buffer.applyGainRamp(c, 0, buffer.getNumSamples(),
                    1-startRatio, 1 - chainFadeRatio);
                buffer.addFromWithRamp(c, 0,
                                       dryCopy.getReadPointer(c), buffer.getNumSamples(),
                                       startRatio, chainFadeRatio);
            }

            // push to fifo so we deallocate the old chain on the message thread
            if (almostEqual(chainFadeRatio, 0.f)) {
                chainsToDeallocate.enqueue(*oldChain);
                oldChain.reset();
            }
        }
    }

private:
    ProcessorChain activeChain;
    std::optional<ProcessorChain> oldChain;
    float chainFadeRatio{1};
    float chainFadePerSample{0};

    float chainFadeTime{0.02};

    moodycamel::ConcurrentQueue<ProcessorChain> chainsToPrepare{4};
    moodycamel::ReaderWriterQueue<ProcessorChain, 4> preparedChains;

    juce::AudioSampleBuffer dryCopy;
    moodycamel::ReaderWriterQueue<ProcessorChain, 4> chainsToDeallocate;

    void timerCallback() override {
        ProcessorChain tempChain;
        while (chainsToPrepare.try_dequeue(tempChain)) {
            for (auto &processor: tempChain) {
                prepareProcessor(*processor);
            }

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

    void renderProcessor(Processor &p, juce::AudioSampleBuffer &buffer, juce::MidiBuffer &m) {
        p.setPlayHead(getPlayHead());
        p.processBlock(buffer, m);
    }

    void numChannelsChanged() override {
        for (const auto &processor: activeChain) {
            prepareProcessor(*processor);
        }
    }
};
