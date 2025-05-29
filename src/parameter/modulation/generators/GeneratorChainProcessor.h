//
// Created by August Pemberton on 08/05/2025.
//

#pragma once
#include "ModGenerator.h"
#include "imagiro_processor/imagiro_processor.h"
#include "juce_audio_processors/juce_audio_processors.h"

class GeneratorChainProcessor : public Processor, juce::Timer {
public:
    using GeneratorChain = std::vector<std::shared_ptr<ModGenerator> >;

    GeneratorChainProcessor()
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
    void queueChain(const GeneratorChain &chain) {
        chainsToPrepare.enqueue(chain);
    }

    void prepareToPlay(double sampleRate, int blockSize) override {
        Processor::prepareToPlay(sampleRate, blockSize);
        for (const auto &g: activeChain) {
            prepareGenerator(*g);
        }
    }

    void process(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) override {
        bool newChainAvailable = false;
        GeneratorChain newChain;
        while (preparedChains.try_dequeue(newChain)) { newChainAvailable = true; }

        if (newChainAvailable) {
            chainsToDeallocate.enqueue(activeChain);
            activeChain = newChain;
        }

        for (const auto &generator: activeChain) {
            processGenerator(*generator, buffer, midiMessages);
        }
    }

private:
    GeneratorChain activeChain;

    moodycamel::ConcurrentQueue<GeneratorChain> chainsToPrepare{4};
    moodycamel::ReaderWriterQueue<GeneratorChain, 4> preparedChains;

    moodycamel::ReaderWriterQueue<GeneratorChain, 4> chainsToDeallocate;

    void timerCallback() override {
        GeneratorChain tempChain;
        while (chainsToPrepare.try_dequeue(tempChain)) {
            for (auto &generator: tempChain) {
                prepareGenerator(*generator);
            }

            preparedChains.enqueue(tempChain);
        }

        // take deallocate chains off the fifo, so they deallocate here once they go out of scope
        while (chainsToDeallocate.try_dequeue(tempChain)) {
        }
    }

    void prepareGenerator(ModGenerator &g) const {
        g.setPlayConfigDetails(
            0, 1,
            getSampleRate(), getBlockSize());
        g.prepareToPlay(getSampleRate(), getBlockSize());
    }

    static void processGenerator(ModGenerator& g, juce::AudioSampleBuffer &buffer, juce::MidiBuffer &m) {
        g.process(buffer, m);
    }

    void numChannelsChanged() override {
        for (const auto &generator: activeChain) {
            prepareGenerator(*generator);
        }
    }
};
