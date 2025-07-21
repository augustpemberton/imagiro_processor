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
        : ModGenerator(CurveLFOGeneratorParameters::PARAMETERS_YAML, m, uid, name, false),
        synth(synth)
    {
        lfo.setFrequency(0.5);
        for (auto& v : voiceLFOs) {
            v.setFrequency(0.5);
        }

        synth.addListener(this);

        frequency->addListener(this);
        phase->addListener(this);
        playbackMode->addListener(this);
        bipolar->addListener(this);

        source.setBipolar(bipolar->getBoolValue());

        const auto defaultCurve = Curve({
            CurvePoint{Point2D{0.f, 0.f}, 0.f},
            CurvePoint{Point2D{0.5f, 1.f}, 0.f},
            CurvePoint{Point2D{1.f, 0.f}, 0.f}
        });

        valueData.set("curve", Serializer<Curve>::serialize(defaultCurve), true);
        startTimer(1, 1.0 / 90.0);
    }

    ~CurveLFOGenerator() override {
        synth.removeListener(this);
        frequency->removeListener(this);
        phase->removeListener(this);
        playbackMode->removeListener(this);
        bipolar->removeListener(this);
    }

    void onVoiceStarted(const size_t voiceIndex) override {
        // if we haven't started this per-voice LFO yet, initialize it to a random phase
        if (!activeVoices.contains(voiceIndex)) {
            voiceLFOs[voiceIndex].setPhase(rand01());
        }

        activeVoices.insert(voiceIndex);

        // retrigger
        if (playbackMode->getProcessorValue() != 1) {
            if (!mono->getBoolValue()) {
                voiceLFOs[voiceIndex].setPhase(0);
            } else {
                lfo.setPhase(0);
            }
        }
    }

    void onVoiceFinished(const size_t voiceIndex) override {
        source.setVoiceValue(0, voiceIndex);
        activeVoices.erase(voiceIndex);

        if (!mono->getBoolValue() && activeVoices.empty()) {
            mostRecentPhase = -1;
            phaseUpdated = true;
        }
    }

    void prepareToPlay(const double sampleRate, const int maximumExpectedSamplesPerBlock) override {
        Processor::prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);
        lfo.setSampleRate(sampleRate);
        for (auto& v : voiceLFOs) {
            v.setSampleRate(sampleRate);
        }
    }

    void OnValueDataUpdated(ValueData &s, const std::string &key, const choc::value::ValueView &newValue) override {
        if (key == "curve") {
            const auto curve = Serializer<Curve>::load(newValue);
            lfo.setTable(curve.getLookupTable());
            for (auto& v : voiceLFOs) {
                v.setTable(curve.getLookupTable());
            }
        }
    }

    void timerCallback(const int timerID) override {
        if (timerID == 1) {
            if (phaseUpdated) {
                phaseUpdated = false;
                emitMessage("setDisplayPhase", choc::value::Value(mostRecentPhase.load()));
            }
        } else Processor::timerCallback(timerID);
    }

protected:
    Parameter* depth { getParameter("depth") };
    Parameter* frequency { getParameter("frequency") };
    Parameter* phase { getParameter("phase") };
    Parameter* playbackMode { getParameter("playbackMode") };
    Parameter* mono { getParameter("mono") };
    Parameter* syncToHost { getParameter("syncToHost") };
    Parameter* bipolar { getParameter("bipolar") };

    std::atomic<float> mostRecentPhase {0};
    std::atomic<bool> phaseUpdated {false};

    void parameterChangedSync(Parameter* p) override {
        ModGenerator::parameterChangedSync(p);
        if (p == frequency) {
            lfo.setFrequency(p->getProcessorValue());
            for (auto voiceIndex = 0; voiceIndex < MAX_MOD_VOICES; ++voiceIndex) {
                voiceLFOs[voiceIndex].setFrequency(p->getProcessorValue(voiceIndex));
            }
        } else if (p == phase) {
            lfo.setPhaseOffset(p->getProcessorValue());
            for (auto voiceIndex = 0; voiceIndex < MAX_MOD_VOICES; ++voiceIndex) {
                voiceLFOs[voiceIndex].setPhaseOffset(p->getProcessorValue(voiceIndex));
            }
        } else if (p == playbackMode) {
            const bool envMode = p->getProcessorValue() == 2;
            lfo.setStopAfterOneOscillation(envMode);
            for (auto voiceIndex = 0; voiceIndex < MAX_MOD_VOICES; ++voiceIndex) {
                voiceLFOs[voiceIndex].setStopAfterOneOscillation(envMode);
            }
        } else if (p == mono) {
            if (!p->getBoolValue()) mostRecentPhase = -1;
        } else if (p == bipolar) {
            source.setBipolar(bipolar->getBoolValue());
        }
    }

    void syncLFOToHost(LookupTableLFO& lfo) {
        const auto posInfo = getPosition();
        if (!posInfo) return;
        if (!posInfo->getIsPlaying()) return;
        const auto timeSamplesOpt = posInfo->getTimeInSamples();
        if (!timeSamplesOpt) return;
        const auto timeSamples = *timeSamplesOpt;

        const auto periodSamples = 1.f / frequency->getProcessorValue() * getSampleRate();
        const auto phaseSamples = timeSamples % static_cast<int>(periodSamples);

        const auto phase01 =phaseSamples / static_cast<float>(periodSamples);

        lfo.setPhase(phase01);
    }

    bool isLockedToTransport() const {
        // only lock when set to free
        if (playbackMode->getProcessorValue() != 1) return false;
        // only lock when on a sync mode
        if (frequency->getConfigIndex() == 0) return false;
        // only lock when sync active
        if (!syncToHost->getBoolValue()) return false;
        return true;
    }

    float advanceGlobalValue(const int numSamples = 1) override {
        if (!mono->getBoolValue()) return 0.f;
        if (isLockedToTransport()) syncLFOToHost(lfo);

        auto v = lfo.process(numSamples);
        if (bipolar->getBoolValue()) {
            v -= 0.5;
            v *= 2;
        }
        v *= depth->getProcessorValue();

        if (lfo.getPhase() != mostRecentPhase) {
            mostRecentPhase = lfo.getPhase();
            phaseUpdated = true;
        }
        return v;
    }

    float advanceVoiceValue(const size_t voiceIndex, const int numSamples = 1) override {
        if (mono->getBoolValue()) return 0.f;
        if (isLockedToTransport()) syncLFOToHost(voiceLFOs[voiceIndex]);

        auto v = voiceLFOs[voiceIndex].process(numSamples);
        if (bipolar->getBoolValue()) {
            v -= 0.5;
            v *= 2;
        }
        v *= depth->getProcessorValue(voiceIndex);

        if (voiceIndex == source.getMatrix()->getMostRecentVoiceIndex()) {
            const auto phase = voiceLFOs[voiceIndex].getPhase();
            if (phase != mostRecentPhase) {
                mostRecentPhase = phase;
                phaseUpdated = true;
            }
        }

        return v;
    }

private:
    LookupTableLFO lfo;
    LookupTableLFO voiceLFOs [MAX_MOD_VOICES];

    MPESynth& synth;

    float lastGlobalValue {0.f};
};
