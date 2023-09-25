//
// Created by August Pemberton on 27/11/2022.
//

#pragma once
#include "Modulator.h"

namespace imagiro {
    class RandomModulator : public Modulator {
    public:
        RandomModulator(Processor& p, Parameter* depth, Parameter* freq, Parameter* smooth, bool useHistory)
                : Modulator(Type::Rand, true, useHistory), processor(p), depthParam(depth),
                  freqParam(freq), smoothParam(smooth)
        {
            v = getNewVal();
        }

        float getValue() override {
            return smooth.getCurrentValue();
        }

        void setSampleRate(double sr) override {
            smooth.setSampleRate(sr);
        }

        void prepareParameters() override {
            auto freqVal = freqParam->getModVal();
            auto periodVal = 1 / freqVal;
            delta = 1.f / (periodVal * sampleRate);
            depthParam->updateCache();

            smooth.setTime(smoothParam->getModVal() * periodVal);
        }

        float getNewVal() {
            return 0.5f + (rand01() - 0.5f) * depthParam->cached();
        }

        void advanceSample(juce::AudioSampleBuffer* b, int sampleIndex) override {
            phase += delta;
            if (phase > 1) {
                v = getNewVal();
                smooth.setValue(v);
                phase -= (int) phase;
            }

            smooth.process(1);
        }

        void sync() {
            auto position = processor.getPosition();
            if (!position.hasValue()) return;

            auto paused = !position->getIsPlaying();
            if (paused) return;

            auto t = position->getTimeInSamples();
            if (!t.hasValue()) return;

            auto periodSamples = processor.getLastSampleRate() / freqParam->getModVal();
            auto phaseSamples = *t % (int)periodSamples;
            auto phase01 = phaseSamples / periodSamples;

            phase = phase01 - (int)phase01;
        }

    private:
        Processor& processor;

        Parameter* depthParam;
        Parameter* freqParam;
        Parameter* smoothParam;

        gin::EasedValueSmoother<float> smooth;

        float phase {0};
        float delta {0};
        float v {0.5};
    };
}
