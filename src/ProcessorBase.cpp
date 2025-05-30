#include "ProcessorBase.h"

ProcessorBase::ProcessorBase(const BusesProperties& ioLayouts)
        : AudioProcessor(ioLayouts)
{
}

const juce::String ProcessorBase::getName() const {
#ifdef PLUGIN_NAME
    return PLUGIN_NAME;
#else
    return "myplugin";
#endif
}

bool ProcessorBase::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool ProcessorBase::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool ProcessorBase::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double ProcessorBase::getTailLengthSeconds() const {
    return 0.0;
}

int ProcessorBase::getNumPrograms() {
    return 1;
}

int ProcessorBase::getCurrentProgram() {
    return 0;
}

void ProcessorBase::setCurrentProgram(int index) {
    juce::ignoreUnused(index);
}

const juce::String ProcessorBase::getProgramName(int index) {
    juce::ignoreUnused(index);
    return {};
}

void ProcessorBase::changeProgramName(int index, const juce::String& newName) {
    juce::ignoreUnused(index, newName);
}

void ProcessorBase::prepareToPlay(double sampleRate, int samplesPerBlock) {
    juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void ProcessorBase::releaseResources() { }

juce::AudioProcessor::BusesProperties ProcessorBase::getDefaultProperties() {
    return BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
            .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
            .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
            ;
}

juce::AudioProcessorEditor* ProcessorBase::createEditor() {
    return new juce::GenericAudioProcessorEditor(*this);
}

bool ProcessorBase::isBusesLayoutSupported( const juce::AudioProcessor::BusesLayout& layouts) const {
    if (isMidiEffect()) {
        return true;
    } else {
        if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
            && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
            return false;
    }

    // This checks if the input layout matches the output layout
#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet()) {
        return false;
    }
#endif

    return true;
}
void ProcessorBase::getStateInformation(juce::MemoryBlock& destData) {
    juce::ignoreUnused(destData);
}

void ProcessorBase::setStateInformation(const void* data, int sizeInBytes) {
    juce::ignoreUnused(data, sizeInBytes);
}