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
        : ModGenerator(LFOGeneratorParameters::PARAMETERS_YAML, m, uid, name),
        synth(synth)
    {
        lfo.setFrequency(0.5f);
        lfo.setShape(LFO::Sine);
        synth.addListener(this);

        frequency->addListener(this);
        phase->addListener(this);
    }

    ~LFOGenerator() override {
        synth.removeListener(this);
        frequency->removeListener(this);
        phase->removeListener(this);
    }

    void onVoiceStarted(size_t voiceIndex) override {
        // if we haven't started this per-voice LFO yet, initialize it to a random phase
        if (!activeVoices.contains(voiceIndex)) {
            voiceLFOs[voiceIndex].setPhase(rand01());
        }

        activeVoices.insert(voiceIndex);

        if (!retrigger->getBoolValue()) return;

        if (!mono->getBoolValue()) {
            voiceLFOs[voiceIndex].setPhase(phase->getProcessorValue());
        } else {
            lfo.setPhase(phase->getProcessorValue());
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
    Parameter* mono { getParameter("mono") };

    void parameterChangedSync(Parameter* p) override {
        ModGenerator::parameterChangedSync(p);
        if (p == frequency) {
            lfo.setFrequency(p->getProcessorValue());
            for (const int voiceIndex: activeVoices) {
                voiceLFOs[voiceIndex].setFrequency(p->getProcessorValue(voiceIndex));
            }
        } else if (p == phase) {
            lfo.setPhase(p->getProcessorValue());
            for (const int voiceIndex: activeVoices) {
                voiceLFOs[voiceIndex].setPhaseOffset(p->getProcessorValue(voiceIndex));
            }
        }
    }

    float advanceGlobalValue(const int numSamples = 1) override {
        if (!mono->getBoolValue()) return 0.f;
        return lfo.process(numSamples) * depth->getProcessorValue();
    }

    float advanceVoiceValue(size_t voiceIndex, const int numSamples = 1) override {
        if (mono->getBoolValue()) return 0.f;
        return voiceLFOs[voiceIndex].process(numSamples) * depth->getProcessorValue(voiceIndex);
    }

private:
    LFO lfo;
    LFO voiceLFOs [MAX_MOD_VOICES];

    MPESynth& synth;

    float lastGlobalValue {0.f};
};
