//
// Created by August Pemberton on 21/02/2025.
//

#pragma once

#include "../ModGenerator.h"

#include "Parameters.h"
#include "choc/text/choc_JSON.h"
#include "../../../../synth/MPESynth.h"

class CurveLFOGenerator : public ModGenerator, MPESynth::Listener {
public:
    CurveLFOGenerator(MPESynth& synth, ModMatrix& m, const std::string& uid, const std::string& name)
        : ModGenerator(CurveLFOGeneratorParameters::PARAMETERS_YAML, m, uid, name, true),
        synth(synth)
    {
        lfo.setFrequency(0.5);
        for (auto& v : voiceLFOs) {
            v.setFrequency(0.5);
        }

        synth.addListener(this);

        frequency->addListener(this);
        phase->addListener(this);

        const auto defaultCurve = Curve({
            {0.f, 0.f, 0.25f, 0.5f},
            {0.5f, 1.f, 0.75f, 0.5f},
            {1.f, 0.f, 1.f, 1.f}
        });
        stringData.set("curve", choc::json::toString(defaultCurve.getState()), true);
    }

    ~CurveLFOGenerator() override {
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

    void OnStringDataUpdated(StringData &s, const std::string &key, const std::string &newValue) override {
        if (key == "curve") {
            const auto curve = Curve::fromState(choc::json::parse(newValue));
            lfo.setTable(curve.getLookupTable());
            for (auto& v : voiceLFOs) {
                v.setTable(curve.getLookupTable());
            }
        }
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
            for (auto voiceIndex = 0; voiceIndex < MAX_MOD_VOICES; ++voiceIndex) {
                voiceLFOs[voiceIndex].setFrequency(p->getProcessorValue(voiceIndex));
            }
        } else if (p == phase) {
            lfo.setPhase(p->getProcessorValue());
            for (auto voiceIndex = 0; voiceIndex < MAX_MOD_VOICES; ++voiceIndex) {
                voiceLFOs[voiceIndex].setPhaseOffset(p->getProcessorValue(voiceIndex));
            }
        }
    }

    float advanceGlobalValue(const int numSamples = 1) override {
        if (!mono->getBoolValue()) return 0.f;
        return lfo.process(numSamples) * depth->getProcessorValue();
    }

    float advanceVoiceValue(const size_t voiceIndex, const int numSamples = 1) override {
        if (mono->getBoolValue()) return 0.f;
        return voiceLFOs[voiceIndex].process(numSamples) * depth->getProcessorValue(voiceIndex);
    }

private:
    LookupTableLFO lfo;
    LookupTableLFO voiceLFOs [MAX_MOD_VOICES];

    MPESynth& synth;

    float lastGlobalValue {0.f};
};
