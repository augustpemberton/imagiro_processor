//
// Created by August Pemberton on 21/02/2025.
//

#pragma once

#include "../ModGenerator.h"
#include <imagiro_processor/imagiro_processor.h>

#include "Parameters.h"

class LFOGenerator : public ModGenerator, MPESynth::Listener {
public:
    LFOGenerator(MPESynth& synth, ModMatrix& m, const std::string& uid, const std::string& name)
        : ModGenerator(LFOGeneratorParameters::PARAMETERS_YAML, m, uid, name, true),
        synth(synth)
    {
        lfo.setFrequency(0.5f);
        lfo.setShape(LFO::Sine);
        synth.addListener(this);
    }

    ~LFOGenerator() override {
        synth.removeListener(this);
    }

    void onVoiceStarted(size_t voiceIndex) override {
        // if we haven't started this per-voice LFO yet, initialize it to a random phase
        if (!activeVoices.contains(voiceIndex)) {
            voiceLFOs[voiceIndex].setPhase(rand01());
        }

        activeVoices.insert(voiceIndex);

        if (!retrigger->getBoolValue()) return;
        if (perVoice->getBoolValue()) {
            voiceLFOs[voiceIndex].setPhase(0);
        } else {
            lfo.setPhase(0);
        }
    }

    void onVoiceFinished(const size_t voiceIndex) override {
        source.setVoiceValue(0, voiceIndex);
        activeVoices.erase(voiceIndex);
    }

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override {
        Processor::prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);
        lfo.setSampleRate(sampleRate);
    }

protected:
    Parameter* depth { getParameter("depth") };
    Parameter* frequency { getParameter("frequency") };
    Parameter* phase { getParameter("phase") };
    Parameter* retrigger { getParameter("retrigger") };
    Parameter* perVoice { getParameter("pervoice") };

    float advanceGlobalValue(const int numSamples = 1) override {
        if (perVoice->getBoolValue()) return 0.f;
        lfo.setFrequency(frequency->getProcessorValue());
        lfo.setPhaseOffset(phase->getProcessorValue());
        auto depthVal = depth->getProcessorValue();
        return lfo.process(numSamples) * depthVal;
    }

    float advanceVoiceValue(size_t voiceIndex, int numSamples = 1) override {
        if (!perVoice->getBoolValue()) return 0.f;

        voiceLFOs[voiceIndex].setFrequency(frequency->getProcessorValue((int)voiceIndex));
        voiceLFOs[voiceIndex].setPhaseOffset(phase->getProcessorValue((int)voiceIndex));
        auto depthVal = depth->getProcessorValue((int)voiceIndex);
        auto voiceVal = voiceLFOs[voiceIndex].process(numSamples) * depthVal;

        return voiceVal;
    }

private:
    LFO lfo;
    LFO voiceLFOs [MAX_MOD_VOICES];

    MPESynth& synth;

    float lastGlobalValue {0.f};
};
