//
// Created by August Pemberton on 08/05/2025.
//

#pragma once
#include "imagiro_processor/src/Processor.h"
#include "Parameters.h"
#include <juce_dsp/juce_dsp.h>

using namespace imagiro;

class IIRFilterProcessor : public Processor {
public:
    IIRFilterProcessor()
        : Processor(IIRFilterProcessorParameters::PARAMETERS_YAML, ParameterLoader(), getDefaultProperties()) {
        frequencyParam = getParameter("frequency");
        typeParam = getParameter("type");

        typeParam->addListener(this);
    }

    ~IIRFilterProcessor() override {
        typeParam->removeListener(this);
    }

    BusesProperties getDefaultProperties() {
        return BusesProperties()
            .withInput("Input", juce::AudioChannelSet::stereo(), true)
            .withOutput("Output", juce::AudioChannelSet::stereo(), true);
    }

    void prepareToPlay(const double sampleRate, const int samplesPerBlock) override {
        Processor::prepareToPlay(sampleRate, samplesPerBlock);
        prepareFilterSpec();
    }

    void numBusesChanged() override {
        prepareFilterSpec();
    }

    void prepareFilterSpec() {
        const juce::dsp::ProcessSpec spec{
            getSampleRate(),
            static_cast<juce::uint32>(getBlockSize()),
            static_cast<juce::uint32>(getTotalNumOutputChannels())};
        filter.prepare(spec);

        updateFilter(frequencyParam->getProcessorValue());
    }

    void updateFilter(float freq) const {
        const int type = std::round(typeParam->getProcessorValue());

        DBG("setting up " << freq);
        if (type == 0) {
            *filter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(getSampleRate(), freq);
        } else if (type == 1) {
            *filter.state = *juce::dsp::IIR::Coefficients<float>::makeBandPass(getSampleRate(), freq);
        } else if (type == 2) {
            *filter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(getSampleRate(), freq);
        }
    }

    void parameterChanged(Parameter *p) override {
        Processor::parameterChanged(p);
        reloadFilterFlag = true;
    }

    void process(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) override {
        auto smoothingValue = frequencyParam->getSmoothedValue(0);
        auto targetValue = frequencyParam->getProcessorValue();

        if (!reloadFilterFlag && almostEqual(smoothingValue, targetValue)) {
            juce::dsp::AudioBlock<float> block(buffer);
            const juce::dsp::ProcessContextReplacing context(block);
            filter.process(context);
        } else {
            reloadFilterFlag = false;
            for (auto s=0; s<buffer.getNumSamples(); s++) {
                updateFilter(frequencyParam->getSmoothedValue(s));

                juce::dsp::AudioBlock block(buffer.getArrayOfWritePointers(),
                    buffer.getNumChannels(), s, 1);
                const juce::dsp::ProcessContextReplacing context(block);

                filter.process(context);
            }
        }


    }

private:
    Parameter *frequencyParam;
    Parameter *typeParam;

    std::atomic<bool> reloadFilterFlag {false};

    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float> > filter;
};
