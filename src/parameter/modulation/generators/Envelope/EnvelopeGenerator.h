//
// Created by August Pemberton on 21/02/2025.
//

#pragma once

#include "../ModGenerator.h"

#include "Parameters.h"
#include "imagiro_processor/src/synth/MPESynth.h"

class EnvelopeGenerator : public ModGenerator, MPESynth::Listener {
public:
    EnvelopeGenerator(MPESynth &synth, ModMatrix &m, const std::string &uid, const std::string &name)
        : ModGenerator(EnvelopeGeneratorParameters::PARAMETERS_YAML, m, uid, name),
          synth(synth) {
        synth.addListener(this);
        attack->addListener(this);
        decay->addListener(this);
        sustain->addListener(this);
        release->addListener(this);
    }

    ~EnvelopeGenerator() override {
        synth.removeListener(this);
        attack->removeListener(this);
        decay->removeListener(this);
        sustain->removeListener(this);
        release->removeListener(this);
    }

    void prepareToPlay(const double sampleRate, const int maximumExpectedSamplesPerBlock) override {
        Processor::prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);
        for (int i=0; i<MAX_MOD_VOICES; i++) {
            envelope[i].setSampleRate(sampleRate);
            envelope[i].setMaxBlockSize(maximumExpectedSamplesPerBlock);
        }
    }

    void onVoiceStarted(const size_t voiceIndex) override {
        activeVoices.insert(voiceIndex);
        envelope[voiceIndex].noteOn();
    }

    void onVoiceReleased(const size_t voiceIndex) override {
        activeVoices.insert(voiceIndex);
        envelope[voiceIndex].noteOff();
    }

protected:
    Parameter *attack{getParameter("attack")};
    Parameter *decay{getParameter("decay")};
    Parameter *sustain{getParameter("sustain")};
    Parameter *release{getParameter("release")};

    Envelope envelope [MAX_MOD_VOICES];
    MPESynth &synth;

    void parameterChanged(Parameter *) override {
        for (int i=0; i<MAX_MOD_VOICES; i++) {
            envelope[i].setParameters({
                attack->getProcessorValue(),
                decay->getProcessorValue(),
                sustain->getProcessorValue(),
                release->getProcessorValue()
            });
        }
    }

    float advanceGlobalValue(const int numSamples = 1) override {
        return 0.f;
    }

    float advanceVoiceValue(const size_t voiceIndex, const int numSamples = 1) override {
        float val = 0;
        for (int i=0; i<numSamples; i++) {
            val = envelope[voiceIndex].getNextSample();
        }

        if (!envelope[voiceIndex].isActive()) {
            activeVoices.erase(voiceIndex);
        }

        return val;
    }
};
