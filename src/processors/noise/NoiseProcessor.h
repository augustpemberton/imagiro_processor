//
// Created by August Pemberton on 08/05/2025.
//

#pragma once
#include "imagiro_processor/src/Processor.h"
#include "Parameters.h"
#include "../../grain/GrainBuffer.h"
#include "../../grain/Grain.h"
#include "imagiro_processor/src/grain/GrainBuffer.h"

using namespace imagiro;
class NoiseProcessor : public Processor, GrainBuffer::Listener {
public:
    NoiseProcessor()
        : Processor(NoiseProcessorParameters::PARAMETERS_YAML,
            ParameterLoader(), getDefaultProperties()),
        grain(grainBuffer)
    {
        tightnessParam->addListener(this);

        env.setAttackMs(tightnessParam->getProcessorValue() * 1000);
        env.setReleaseMs(tightnessParam->getProcessorValue() * 1000);

        lpFilterProcessor.getParameter("type")->setUserValue(0);
        hpFilterProcessor.getParameter("type")->setUserValue(2);

        lpFreq = lpFilterProcessor.getParameter("frequency");
        hpFreq = hpFilterProcessor.getParameter("frequency");

        lpFreq->setUserValue(lowpassParam->getUserValue());
        hpFreq->setUserValue(highpassParam->getUserValue());

        lowpassParam->addListener(this);
        highpassParam->addListener(this);

        grain.configure({
            -1, 0, {
                0, 1, 0.01, true
            }
        });
        grainBuffer.addListener(this);
    }

    void loadFilePath(const std::string& path) {
        const juce::File file (path);
        if (!file.exists()) return;

        grain.stop(false);
        grainBuffer.loadFileIntoBuffer(file, lastSampleRate);
    }

    choc::value::Value OnMessageReceived(std::string type, const choc::value::ValueView &data) override {
        if (type == "loadFile") {
            loadFilePath(data.getWithDefault(""));
        }
        return {};
    }

    void OnBufferUpdated(GrainBuffer &) override {
        playGrainFlag = true;
        getStringData().set("filePath", grainBuffer.getLastLoadedFile().getFullPathName().toStdString(), true);
    }

    void OnStringDataUpdated(StringData &s, const std::string &key, const std::string &newValue) override {
        if (key == "filePath") {
            if (grainBuffer.getLastLoadedFile().getFullPathName().toStdString() != newValue) {
                loadFilePath(newValue);
            }
        }
    }

    static BusesProperties getDefaultProperties() {
        return BusesProperties()
            .withInput("Input", juce::AudioChannelSet::stereo(), true)
            .withOutput("Output", juce::AudioChannelSet::stereo(), true);
    }

    void parameterChanged(Parameter* p) override {
        Processor::parameterChanged(p);
        if (p == tightnessParam) {
            env.setReleaseMs(p->getProcessorValue() * 1000);
            env.setAttackMs(p->getProcessorValue() * 1000);
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
        noiseBuffer.clear();
        grain.prepareToPlay(sampleRate, samplesPerBlock);
    }

    void fillNoiseBuffer(int numSamples) {
        if (typeParam->getProcessorValue() == 0) {
            for (auto c = 0; c < noiseBuffer.getNumChannels(); c++) {
                for (auto s = 0; s < numSamples; s++) {
                    const auto noiseSample = rand01() * 2 - 1;
                    noiseBuffer.setSample(c, s, noiseSample);
                }
            }
        } else {
            grain.processBlock(noiseBuffer, 0, numSamples, true);
        }
    }

    void filterNoiseBuffer() {
        juce::MidiBuffer temp;
        lpFilterProcessor.processBlock(noiseBuffer, temp);
        hpFilterProcessor.processBlock(noiseBuffer, temp);
    }


    void process(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) override {
        // if we're on file mode but no file loaded, don't do anything
        if (typeParam->getProcessorValue() > 0 &&  grainBuffer.getBuffer()->getNumSamples() == 0) {
            return;
        }

        if (playGrainFlag) {
            playGrainFlag = false;
            grain.play();
        }

        fillNoiseBuffer(buffer.getNumSamples());
        filterNoiseBuffer();

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
    Parameter *gainParam { getParameter("gain") };
    Parameter* typeParam { getParameter("type") };
    Parameter *tightnessParam { getParameter("tightness") } ;

    Parameter *lowpassParam { getParameter("lowpass") };
    Parameter *highpassParam { getParameter("highpass") };

    Parameter *lpFreq;
    Parameter *hpFreq;

    GrainBuffer grainBuffer;
    Grain grain;

    EnvelopeFollower<float> env {50, 150};
    juce::SmoothedValue<float> gateGain;

    IIRFilterProcessor lpFilterProcessor;
    IIRFilterProcessor hpFilterProcessor;

    juce::AudioSampleBuffer noiseBuffer;

    bool playGrainFlag {false};
};
