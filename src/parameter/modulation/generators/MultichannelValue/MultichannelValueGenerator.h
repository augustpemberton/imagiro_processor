//
// Created by August Pemberton on 21/02/2025.
//

#pragma once

#include "../ModGenerator.h"
#include <imagiro_processor/imagiro_processor.h>

#include "MultichannelValueSources.h"
#include "Parameters.h"

class MultichannelValueGenerator : public ModGenerator {
public:
    MultichannelValueGenerator(MultichannelValueSources& s, ModMatrix &m, const std::string &uid, const std::string &name)
        : ModGenerator(MultichannelValueGeneratorParameters::PARAMETERS_YAML, m, uid, name), sources(s) {

        for (const auto&[value, name] : sources.getSources()) {
            sourceParam->getConfig()->choices.push_back(name);
        }

        activeSource = &sources.getSources()[0];
        sourceParam->addListener(this);
        setSourceFromIndex(sourceParam->getProcessorValue(), true);
    }

    ~MultichannelValueGenerator() override {
        sourceParam->removeListener(this);
    }

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override {
        Processor::prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);
        //
    }


protected:
    Parameter *sourceParam {getParameter("source")};
    MultichannelValueSources& sources;

    const NamedMultichannelValue* activeSource {nullptr};
    const NamedMultichannelValue* queuedActiveSource {nullptr};

    void parameterChanged(Parameter* p) override {
        setSourceFromIndex(p->getProcessorValue());
    }

    float advanceGlobalValue(const int numSamples = 1) override {
        activeVoices = sources.getActiveVoices();
        if (queuedActiveSource != nullptr) {
            activeSource = queuedActiveSource;
            source.setBipolar(activeSource->value.getBipolar());
            queuedActiveSource = nullptr;
        }

        if (!activeSource) return 0.f;

        return activeSource->value.getGlobalValue();
    }

    float advanceVoiceValue(const size_t voiceIndex, int numSamples = 1) override {
        if (!activeSource) return 0.f;
        return activeSource->value.getVoiceValue(voiceIndex);
    }

    void setSourceFromIndex(const float index, const bool force = false) {
        const auto& source = sources.getSources()[index];
        if (&source != activeSource || force) {
            setSource(source);
        }
    }

    void setSource(const NamedMultichannelValue& v) {
        queuedActiveSource = &v;
    }
};
