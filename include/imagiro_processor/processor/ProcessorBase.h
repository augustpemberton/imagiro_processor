// ProcessorBase.h
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace imagiro {

    class ProcessorBase : public juce::AudioProcessor {
    public:
        explicit ProcessorBase(const BusesProperties& ioLayout = getDefaultProperties())
            : AudioProcessor(ioLayout) {}

        const juce::String getName() const override { return JucePlugin_Name; }
        bool acceptsMidi() const override { return JucePlugin_WantsMidiInput; }
        bool producesMidi() const override { return JucePlugin_ProducesMidiOutput; }
        bool isMidiEffect() const override { return JucePlugin_IsMidiEffect; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram(int) override {}
        const juce::String getProgramName(int) override { return {}; }
        void changeProgramName(int, const juce::String&) override {}
        bool hasEditor() const override { return true; }
        void releaseResources() override {}
        juce::AudioProcessorEditor *createEditor() override { return new juce::GenericAudioProcessorEditor(*this); }

        bool isBusesLayoutSupported(const BusesLayout& layouts) const override {
            const auto mono = juce::AudioChannelSet::mono();
            const auto stereo = juce::AudioChannelSet::stereo();
            const auto output = layouts.getMainOutputChannelSet();

            if (output != mono && output != stereo) return false;

#if !JucePlugin_IsSynth
            if (layouts.getMainInputChannelSet() != output) return false;
#endif

            return true;
        }

        static BusesProperties getDefaultProperties() {
            return BusesProperties()
                #if !JucePlugin_IsMidiEffect
                #if !JucePlugin_IsSynth
                .withInput("Input", juce::AudioChannelSet::stereo(), true)
                #endif
                .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                #endif
            ;
        }
    };

} // namespace imagiro