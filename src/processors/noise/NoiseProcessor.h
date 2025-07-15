//
// Created by August Pemberton on 08/05/2025.
//

#pragma once
#include "imagiro_processor/src/Processor.h"
#include "Parameters.h"
#include "../../grain/Grain.h"
#include "imagiro_processor/src/bufferpool/FileBufferCache.h"
#include "imagiro_processor/src/dsp/XORRandom.h"
#include "imagiro_processor/src/dsp/filter/CascadedOnePoleFilter.h"
#include "imagiro_processor/src/valuedata/Serialize.h"

using namespace imagiro;
class NoiseProcessor : public Processor, FileBufferCache::Listener {
public:
    NoiseProcessor()
        : Processor(NoiseProcessorParameters::PARAMETERS_YAML,
            ParameterLoader(), getDefaultProperties()),
        grain(sampleDataBuffer)
    {
        tightnessParam->addListener(this);

        env.setAttackMs(tightnessParam->getProcessorValue() * 1000);
        env.setReleaseMs(tightnessParam->getProcessorValue() * 1000);

        lowpassParam->addListener(this);
        highpassParam->addListener(this);

        pitchParam = getParameter("pitch");
        pitchParam->addListener(this);

        grain.configure({
            -1, 0, {
                0, 1, 0.01, true
            }
        });
        fbc.addListener(this);

        filePath.setOnChanged([&](auto& path) {
            loadFilePath(path);
        });
    }

    ~NoiseProcessor() override {
        fbc.removeListener(this);
    }

    void loadFilePath(const std::string& path) {
        const juce::File file (path);
        if (!file.exists()) return;

        fbc.requestBuffer(path);
    }

    void onBufferLoaded(const BufferCacheKey& key, const std::shared_ptr<InfoBuffer> buffer) override {
        if (key.path == filePath.get()) {
            loadedBuffer = buffer;
            playGrainFlag = true;
        }
    }

    static BusesProperties getDefaultProperties() {
        return BusesProperties()
            .withInput("Input", juce::AudioChannelSet::stereo(), true)
            .withOutput("Output", juce::AudioChannelSet::stereo(), true);
    }

    void parameterChangedSync(Parameter* p) override {
        Processor::parameterChanged(p);
        if (p == tightnessParam) {
            env.setReleaseMs(p->getProcessorValue() * 1000);
            env.setAttackMs(p->getProcessorValue() * 1000);
        } else if (p == lowpassParam) {
            lpFilter.setCutoff(p->getUserValue());
        } else if (p == highpassParam) {
            hpFilter.setCutoff(p->getUserValue());
        } else if (p == pitchParam) {
            grain.setPitch(p->getProcessorValue());
        }
    }

    void prepareToPlay(const double sampleRate, const int samplesPerBlock) override {
        Processor::prepareToPlay(sampleRate, samplesPerBlock);
        env.setSampleRate(sampleRate);
        gateGain.reset(static_cast<int>(sampleRate * 0.01));

        lpFilter.setSampleRate(sampleRate);
        hpFilter.setSampleRate(sampleRate);

        lpFilter.setChannels(getTotalNumInputChannels());
        hpFilter.setChannels(getTotalNumInputChannels());

        noiseBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock);
        noiseBuffer.clear();
        sampleDataBuffer.resize(samplesPerBlock);
        grain.prepareToPlay(sampleRate, samplesPerBlock);
    }

    bool isFileLoaded() const {
        return loadedBuffer != nullptr;
    }

    void fillNoiseBuffer(const int numSamples) {
        if (typeParam->getProcessorValue() == 0) {
            for (auto c = 0; c < noiseBuffer.getNumChannels(); c++) {
                for (auto s = 0; s < numSamples; s++) {
                    const auto noiseSample = random.nextFloat() * 0.1;
                    noiseBuffer.setSample(c, s, noiseSample);
                }
            }
        } else {
            if (isFileLoaded()) grain.processBlock(noiseBuffer, 0, numSamples, true);
            else noiseBuffer.clear();
        }
    }

    void filterNoiseBuffer() {
        for (auto c=0; c<noiseBuffer.getNumChannels(); c++) {
            for (auto s=0; s<noiseBuffer.getNumSamples(); s++) {
                const auto sample = noiseBuffer.getSample(c, s);
                const auto filteredSample = lpFilter.processLP(hpFilter.processHP(sample, c), c);
                noiseBuffer.setSample(c, s, filteredSample);
            }
        }
    }


    void process(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) override {

        if (playGrainFlag) {
            playGrainFlag = false;
            grain.stop(false);
            grain.play();
        }

        fillNoiseBuffer(buffer.getNumSamples());
        filterNoiseBuffer();

        static constexpr auto noiseNormalizationGain = 1;
        for (auto s = 0; s < buffer.getNumSamples(); s++) {
            float monoSample {0};
            const float gateValue = gateGain.getNextValue();
            for (auto c = 0; c < buffer.getNumChannels(); c++) {
                const auto drySample = buffer.getSample(c, s);
                monoSample += drySample;

                auto noiseSample = noiseBuffer.getSample(c%noiseBuffer.getNumChannels(), s);
                noiseSample *= gateValue;
                noiseSample *= gainParam->getSmoothedValue(s);
                noiseSample *= noiseNormalizationGain;

                const auto outputSample = drySample + noiseSample;
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

    Parameter *pitchParam;

    Grain grain;

    EnvelopeFollower<> env {50, 150};
    juce::SmoothedValue<float> gateGain;

    CascadedOnePoleFilter<> lpFilter;
    CascadedOnePoleFilter<> hpFilter;

    juce::AudioSampleBuffer noiseBuffer;
    std::vector<GrainSampleData> sampleDataBuffer;

    FileBufferCache fbc;
    SerializableValue<std::string> filePath {valueData, "filePath", "", true};
    std::shared_ptr<InfoBuffer> loadedBuffer;

    bool playGrainFlag {false};

    XORRandom random;
};
