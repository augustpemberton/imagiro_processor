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
                 const std::string &name = "")
        : Processor(parametersYAML, ParameterLoader(), BusesProperties()),
          source(name, &m) {

        auto bp = std::move(
            ParameterLoader().loadParameters(
                " bypass: {name: \"bypass\", default: 0, type: toggle}",
                *this)[0]);
        this->addParam(std::move(bp));

        bypassParam = getParameter("bypass");
    }


    void process(juce::AudioSampleBuffer& buffer, juce::MidiBuffer& midiMessages) override {
        if (bypassParam && bypassParam->getBoolValue()) {
            source.setGlobalValue(0);
            for (const auto &voiceIndex: activeVoices) {
                source.setVoiceValue(0, voiceIndex);
            }
            return;
        }

        source.setGlobalValue(advanceGlobalValue(buffer.getNumSamples()));
        // copy active voices in case they get deleted in the loop
        auto activeVoicesCopy = activeVoices;
        for (const auto &voiceIndex: activeVoicesCopy) {
            source.setVoiceValue(advanceVoiceValue(voiceIndex, buffer.getNumSamples()), voiceIndex);
        }
    }

    [[maybe_unused]] ModSource& getSource() { return source; }

protected:
    ModSource source;

    virtual float advanceGlobalValue(int numSamples = 1) = 0;
    virtual float advanceVoiceValue(size_t voiceIndex, int numSamples = 1) = 0;

    FixedHashSet<size_t, MAX_MOD_VOICES> activeVoices;

    Parameter* bypassParam;
};
