//
// Created by August Pemberton on 22/09/2022.
//

#pragma once
#include "Modulator.h"
#include "../EnvelopeFollower.h"
#include "EnvelopeSource.h"

namespace imagiro {
    class EnvFollowModulator : public Modulator, public EnvelopeSource::Listener {
    public:
        EnvFollowModulator(Parameter* attack, Parameter* release, Parameter* gain,
                           Parameter* source,
                           juce::OwnedArray<EnvelopeSource>& sources, bool useHistory)
                : Modulator(Modulator::Type::Env, false, useHistory), envelopeSources(sources), attackParam(attack),
                releaseParam(release), gainParam(gain), sourceParam(source) {

            if (!envelopeSources.isEmpty())
                setSource(envelopeSources[0]);
        }

        ~EnvFollowModulator() {
            if (selectedSource) selectedSource->removeListener(this);
        }

        void setSource(EnvelopeSource* s) {
            if (s == selectedSource) return;
            if (selectedSource) selectedSource->removeListener(this);
            selectedSource = s;
            selectedSource->addListener(this);

            DBG(selectedSource->getName());

        }

        void setAttack(float seconds) { env.setAttackMs(seconds * 1000); }
        void setRelease(float seconds) { env.setReleaseMs(seconds * 1000); }

        float getValue() override {
            return juce::jlimit(0.f, 1.f, env.getEnvelope() *
                                          juce::Decibels::decibelsToGain(gainVal));
        }

        void prepareParameters() override {
            setAttack(attackParam->getModVal());
            setRelease(releaseParam->getModVal());

            int sourceIndex = sourceParam->getModVal();
            if (sourceIndex < envelopeSources.size())
                setSource(envelopeSources[sourceIndex]);

            gainVal = gainParam->getModVal();
        }

        void envelopeUpdated(EnvelopeSource* source, juce::Array<float>& sample) override {
            float monoSample = 0.f;

            for (float c : sample) monoSample += c;
            monoSample /= (float)sample.size();

            env.pushSample(monoSample);
        }

        void advanceSample(juce::AudioSampleBuffer* b, int sampleIndex) override {
            //
        }

        void setSampleRate(double sr) override {
            Modulator::setSampleRate(sr);
            env.setSampleRate(sr);
        }

    private:
        float gainVal;

        EnvelopeSource* selectedSource {nullptr};
        juce::OwnedArray<EnvelopeSource>& envelopeSources;

        EnvelopeFollower env;
        Parameter* attackParam;
        Parameter* releaseParam;
        Parameter* gainParam;
        Parameter* sourceParam;
    };
}


