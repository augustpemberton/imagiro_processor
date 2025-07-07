//
// Created by August Pemberton on 08/05/2025.
//

#pragma once
#include "imagiro_processor/src/Processor.h"
#include "Parameters.h"
#include "./Diffuser.h"

using namespace imagiro;
class DiffuseDelayProcessor : public Processor {
public:
    DiffuseDelayProcessor()
        : Processor(DiffuseDelayProcessorParameters::PARAMETERS_YAML, ParameterLoader(), getDefaultProperties()) {
        diffuseParam = getParameter("diffuse");
        delayParam = getParameter("delay");
        feedbackParam = getParameter("feedback");
        lowpassParam = getParameter("diffuseLowpass");
        highpassParam = getParameter("diffuseHighpass");
        modDepthParam = getParameter("diffuseModDepth");
        modRateParam = getParameter("diffuseModRate");

        delayParam->setSmoothTime(0.02);

        for (auto p : {
                diffuseParam, delayParam, feedbackParam,
                lowpassParam, highpassParam,
                modDepthParam, modRateParam
        }) {
                    p->addListener(this);
        }

        auto skewLength = diffuseParam->getUserValue() * delayParam->getUserValue();
        diffuser.setDiffusionLength(delayParam->getUserValue(), skewLength * 2);
        diffuser.setFeedback(feedbackParam->getUserValue());
        diffuser.setLPCutoff(lowpassParam->getUserValue());
        diffuser.setHPCutoff(highpassParam->getUserValue());
        diffuser.setModAmount(modDepthParam->getUserValue());
        diffuser.setModFreq(modRateParam->getUserValue());

    }

    ~DiffuseDelayProcessor() {
        diffuseParam->removeListener(this);
        delayParam->addListener(this);
        feedbackParam->addListener(this);
        lowpassParam->addListener(this);
        highpassParam->addListener(this);
        modDepthParam->removeListener(this);
        modRateParam->removeListener(this);
    }

    BusesProperties getDefaultProperties() {
        return BusesProperties()
            .withInput("Input", juce::AudioChannelSet::stereo(), true)
            .withOutput("Output", juce::AudioChannelSet::stereo(), true);
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override {
        diffuser.prepare(sampleRate, samplesPerBlock);
        Processor::prepareToPlay(sampleRate, samplesPerBlock);
    }

    void process(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) override {
        diffuser.process(buffer);
    }

    void parameterChangedSync(Parameter *param) override {
        Processor::parameterChangedSync(param);
        if (param == diffuseParam || param == delayParam) {
            auto skewLength = diffuseParam->getProcessorValue() * delayParam->getProcessorValue();
            diffuser.setDiffusionLength(delayParam->getProcessorValue(), skewLength * 2);
        } else if (param == feedbackParam) {
            diffuser.setFeedback(feedbackParam->getProcessorValue());
        } else if (param == lowpassParam) {
            diffuser.setLPCutoff(param->getProcessorValue());
        } else if (param == highpassParam) {
            diffuser.setHPCutoff(param->getProcessorValue());
        } else if (param == modDepthParam) {
            diffuser.setModAmount(param->getProcessorValue());
        } else if (param == modRateParam) {
            diffuser.setModFreq(param->getProcessorValue());
        }
    }

private:
    Parameter* diffuseParam;
    Parameter* delayParam;
    Parameter* feedbackParam;
    Parameter* highpassParam;
    Parameter* lowpassParam;
    Parameter* modDepthParam;
    Parameter* modRateParam;

    Diffuser<4, 4> diffuser;
};
