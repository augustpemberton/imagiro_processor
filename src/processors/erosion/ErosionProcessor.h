//
// Created by August Pemberton on 08/05/2025.
//

#pragma once
#include "Parameters.h"
#include "imagiro_processor/src/Processor.h"
#include "imagiro_processor/src/processors/iir-filter/IIRFilterProcessor.h"

using namespace imagiro;
class ErosionProcessor : public Processor {
public:
    ErosionProcessor()
        : Processor(ErosionProcessorParameters::PARAMETERS_YAML,
            ParameterLoader(), getDefaultProperties()) {
        gainParam = getParameter("gain");

        lpFilterProcessor.getParameter("type")->setUserValue(0);
        hpFilterProcessor.getParameter("type")->setUserValue(2);

        lpFreq = lpFilterProcessor.getParameter("frequency");
        hpFreq = hpFilterProcessor.getParameter("frequency");

        lowpassParam = getParameter("lowpass");
        highpassParam= getParameter("highpass");

        lpFreq->setUserValue(lowpassParam->getUserValue());
        hpFreq->setUserValue(highpassParam->getUserValue());

        lowpassParam->addListener(this);
        highpassParam->addListener(this);
    }

    BusesProperties getDefaultProperties() {
        return BusesProperties()
            .withInput("Input", juce::AudioChannelSet::stereo(), true)
            .withOutput("Output", juce::AudioChannelSet::stereo(), true);
    }

    void parameterChanged(Parameter* p) override {
        Processor::parameterChanged(p);
        if (p == lowpassParam) {
            lpFreq->setUserValue(p->getUserValue());
        } else if (p == highpassParam) {
            hpFreq->setUserValue(p->getUserValue());
        }
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override {
        Processor::prepareToPlay(sampleRate, samplesPerBlock);
        env.setSampleRate(sampleRate);

        lpFilterProcessor.setPlayConfigDetails(
            getTotalNumInputChannels(),
            getTotalNumOutputChannels(),
            getSampleRate(), getBlockSize()
            );
        hpFilterProcessor.setPlayConfigDetails(
            getTotalNumInputChannels(),
            getTotalNumOutputChannels(),
            getSampleRate(), getBlockSize()
            );
        lpFilterProcessor.prepareToPlay(sampleRate, samplesPerBlock);
        hpFilterProcessor.prepareToPlay(sampleRate, samplesPerBlock);

        noiseBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock);

        channelDelays.clear();
        for (auto i=0; i<getTotalNumOutputChannels(); i++) {
            channelDelays.emplace_back();
            channelDelays.back().resize(maxDelayDepthSamples);
        }
    }

    void fillNoiseBuffer() {
        for (auto c = 0; c < noiseBuffer.getNumChannels(); c++) {
            for (auto s = 0; s < noiseBuffer.getNumSamples(); s++) {
                auto noiseSample = rand01();
                noiseBuffer.setSample(c, s, noiseSample);
            }
        }

        juce::MidiBuffer temp;
        lpFilterProcessor.processBlock(noiseBuffer, temp);
        hpFilterProcessor.processBlock(noiseBuffer, temp);
    }

    void process(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) override {
        fillNoiseBuffer();
        for (auto s = 0; s < buffer.getNumSamples(); s++) {
            float monoSample {0};
            for (auto c = 0; c < buffer.getNumChannels(); c++) {
                auto drySample = buffer.getSample(c, s);
                monoSample += drySample;

                auto noiseSample = noiseBuffer.getSample(c, s);
                auto delaySamples = noiseSample * maxDelayDepthSamples;

                auto delayedSample = channelDelays[c].write(drySample).read(delaySamples);
                delayedSample *= gainParam->getSmoothedValue(s);

                auto outputSample = drySample + delayedSample;
                buffer.setSample(c, s, outputSample);
            }
            monoSample /= static_cast<float>(buffer.getNumChannels());

            env.pushSample(std::abs(monoSample));
        }
    }

private:
    Parameter *gainParam;

    Parameter *lowpassParam;
    Parameter *highpassParam;
    Parameter *lpFreq;
    Parameter *hpFreq;
    IIRFilterProcessor lpFilterProcessor;
    IIRFilterProcessor hpFilterProcessor;

    EnvelopeFollower<> env {50, 150};

    juce::AudioSampleBuffer noiseBuffer;

    int maxDelayDepthSamples {1500};
    std::vector<signalsmith::delay::Delay<float>> channelDelays;
};
