//
// Created by August Pemberton on 08/05/2025.
//

#pragma once
#include "imagiro_processor/src/Processor.h"
#include "Parameters.h"

using namespace imagiro;
class NoiseProcessor : public Processor {
public:
    NoiseProcessor()
        : Processor(NoiseProcessorParameters::PARAMETERS_YAML,
            ParameterLoader(), getDefaultProperties()) {
        gainParam = getParameter("gain");
        attackParam = getParameter("attack");
        releaseParam = getParameter("release");

        attackParam->addListener(this);
        releaseParam->addListener(this);

        env.setAttackMs(attackParam->getProcessorValue() * 1000);
        env.setReleaseMs(releaseParam->getProcessorValue() * 1000);

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
        if (p == attackParam) {
            env.setAttackMs(p->getProcessorValue() * 1000);
        } else if (p == releaseParam) {
            env.setReleaseMs(p->getProcessorValue() * 1000);
        } else if (p == lowpassParam) {
            lpFreq->setUserValue(p->getUserValue());
        } else if (p == highpassParam) {
            hpFreq->setUserValue(p->getUserValue());
        }
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override {
        Processor::prepareToPlay(sampleRate, samplesPerBlock);
        env.setSampleRate(sampleRate);
        gateGain.reset(static_cast<int>(sampleRate * 0.01));

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
    }

    void fillNoiseBuffer() {
        for (auto c = 0; c < noiseBuffer.getNumChannels(); c++) {
            for (auto s = 0; s < noiseBuffer.getNumSamples(); s++) {
                auto noiseSample = rand01() * 2 - 1;
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
            float gateValue = gateGain.getNextValue();
            for (auto c = 0; c < buffer.getNumChannels(); c++) {
                auto drySample = buffer.getSample(c, s);
                monoSample += drySample;

                auto noiseSample = noiseBuffer.getSample(c, s);
                noiseSample *= gateValue;
                noiseSample *= gainParam->getSmoothedValue(s);

                auto outputSample = drySample + noiseSample;
                buffer.setSample(c, s, outputSample);
            }
            monoSample /= static_cast<float>(buffer.getNumChannels());

            env.pushSample(std::abs(monoSample));
            gateGain.setTargetValue(env.getCurrentValue());
        }
    }

private:
    Parameter *gainParam;
    Parameter *attackParam;
    Parameter *releaseParam;

    Parameter *lowpassParam;
    Parameter *highpassParam;

    Parameter *lpFreq;
    Parameter *hpFreq;

    EnvelopeFollower<float> env {50, 150};
    juce::SmoothedValue<float> gateGain;

    IIRFilterProcessor lpFilterProcessor;
    IIRFilterProcessor hpFilterProcessor;

    juce::AudioSampleBuffer noiseBuffer;
};
