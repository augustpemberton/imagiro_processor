//
// Created by August Pemberton on 21/02/2025.
//

#pragma once

#include "../ModGenerator.h"
#include <imagiro_processor/imagiro_processor.h>

#include "EnvelopeFollowerSources.h"
#include "Parameters.h"

class EnvelopeFollowerGenerator : public ModGenerator {
public:
    EnvelopeFollowerGenerator(EnvelopeFollowerSources &s, ModMatrix &m, const std::string &uid,
                              const std::string &name)
        : ModGenerator(EnvelopeFollowerGeneratorParameters::PARAMETERS_YAML, m, uid, name),
          sources(s)
    {
        for (const auto& source : sources.getSources()) {
            sourceParam->getConfig()->choices.push_back(source->getName());
        }

        activeSource = sources.getSources()[0].get();

        attackParam->addListener(this);
        releaseParam->addListener(this);
        sourceParam->addListener(this);

        env.setAttackMs(attackParam->getProcessorValue() * 1000);
        env.setReleaseMs(releaseParam->getProcessorValue() * 1000);
        setSourceFromIndex(sourceParam->getProcessorValue());
    }

    ~EnvelopeFollowerGenerator() override {
        attackParam->removeListener(this);
        releaseParam->removeListener(this);
        sourceParam->removeListener(this);
    }

    void prepareToPlay(const double sampleRate, const int maximumExpectedSamplesPerBlock) override {
        Processor::prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);
        env.setSampleRate(sampleRate);
    }

    void setSourceFromIndex(float sourceVal) {
        const auto& source = sources.getSources()[sourceVal];
        if (!source) {
            jassertfalse;
            return;
        }

        if (source.get() != activeSource) {
            setSource(*source);
        }
    }

    void setSource(EnvelopeFollowerSource& s) {
        queuedActiveSource = &s;
    }

    void parameterChanged(Parameter* p) override {
        if (p == attackParam) env.setAttackMs(p->getProcessorValue() * 1000);
        else if (p == releaseParam) env.setReleaseMs(p->getProcessorValue() * 1000);
        else if (p == sourceParam) setSourceFromIndex(p->getProcessorValue());
    }


protected:
    Parameter *attackParam {getParameter("attack")};
    Parameter *releaseParam {getParameter("release")};
    Parameter *sourceParam {getParameter("source")};

    EnvelopeFollowerSources &sources;
    EnvelopeFollowerSource* activeSource {nullptr};

    EnvelopeFollowerSource* queuedActiveSource {nullptr};

    EnvelopeFollower<float> env;

    float advanceGlobalValue(const int numSamples = 1) override {
        if (queuedActiveSource != nullptr) {
            activeSource = queuedActiveSource;
            queuedActiveSource = nullptr;
        }

        if (!activeSource) return 0.f;

        for (auto s=0; s<numSamples; s++) {
            const auto sample = activeSource->getBuffer().getSample(0, s);
            env.pushSample(sample);
        }

        auto val = std::clamp(env.getCurrentValue(), 0.f, 1.f);
        val = fastpow(val, 0.588);
        return val;
    }

    float advanceVoiceValue(size_t voiceIndex, int numSamples = 1) override {
        return 0.f;
    }
};
