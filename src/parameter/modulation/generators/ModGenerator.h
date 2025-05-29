//
// Created by August Pemberton on 21/02/2025.
//

#pragma once
#include <imagiro_processor/imagiro_processor.h>

class ModGenerator : public Processor {
public:
    ModGenerator(const juce::String &parametersYAML,
                 ModMatrix &m,
                 const std::string &uid,
                 const std::string &name = "",
                 const ModMatrix::SourceType sourceType = ModMatrix::SourceType::Misc,
                 const bool bipolar = false)
        : Processor(parametersYAML, ParameterLoader(),
            BusesProperties().withOutput("out",juce::AudioChannelSet::mono())),
          source(uid, name, &m, sourceType, bipolar) {
    }

    void process(juce::AudioSampleBuffer& buffer, juce::MidiBuffer& midiMessages) override {
        source.setGlobalValue(advanceGlobalValue(buffer.getNumSamples()));
        for (const auto &voiceIndex: activeVoices) {
            source.setVoiceValue(advanceVoiceValue(voiceIndex, buffer.getNumSamples()), voiceIndex);
        }
    }

    [[maybe_unused]] ModSource& getSource() { return source; }

protected:
    ModSource source;

    virtual float advanceGlobalValue(int numSamples = 1) = 0;
    virtual float advanceVoiceValue(size_t voiceIndex, int numSamples = 1) = 0;

    std::unordered_set<size_t> activeVoices;
};
