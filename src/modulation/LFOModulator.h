//
// Created by August Pemberton on 23/09/2022.
//

#pragma once

namespace imagiro {
    class LFOModulator : public Modulator, public Processor::BPMListener {
    public:
        LFOModulator(Processor& p, Parameter *ampParam, Parameter *freqParam, Parameter *offsetParam, bool useHistory)
                : Modulator(Modulator::Type::LFO, true, useHistory),
                  processor(p), amp(ampParam), freq(freqParam), offset(offsetParam)
        {
            processor.addBPMListener(this);
        }

        ~LFOModulator() {
            processor.removeBPMListener(this);
        }

        float getValue() override {
            return lfo.getValue();
        }

        void playChanged(bool isPlaying) override {
            if (isPlaying) syncLFO();
        }

        void setAmplitude(float a) { lfo.setAmplitude(a); }
        void setFrequency(float f) { lfo.setFrequency(f); }
        void setOffset(float d) { lfo.setOffset(d); }

        void setSampleRate(double sr) override {
            Modulator::setSampleRate(sr);
            lfo.setSampleRate(sr);
            ampSmooth.setSampleRate(sr);
            offsetSmooth.setSampleRate(sr);
        }

        void prepareParameters() override {
            amp->updateCache();
            freq->updateCache();
            offset->updateCache();
        }

        void advanceSample(juce::AudioSampleBuffer* b, int sampleIndex) override {
            ampSmooth.pushSample(amp->cached(), false);
            offsetSmooth.pushSample(offset->cached(), false);

            setAmplitude(ampSmooth.getEnvelope());
            setFrequency(freq->cached());
            setOffset(offsetSmooth.getEnvelope());
            lfo.advance(1);
        }

        void syncLFO() {
            if (freq->getConfig()->name != "sync") return;

            auto position = processor.getPosition();
            if (!position.hasValue()) return;

            auto paused = !position->getIsPlaying();
            if (paused) return;

            auto t = position->getTimeInSamples();
            if (!t.hasValue()) return;

            auto LFOPeriodSamples = processor.getLastSampleRate() / freq->getModVal();
            auto phaseSamples = *t % (int)LFOPeriodSamples;
            auto phase01 = phaseSamples / LFOPeriodSamples;

            phase01 += 0.25;
            phase01 -= (int)phase01;

            lfo.setPhase(phase01);
        }

    protected:
        Processor& processor;

        LFO lfo {1,1,true};
        Parameter* amp;
        Parameter* freq;
        Parameter* offset;

        EnvelopeFollower ampSmooth {100.f, 100.f};
        EnvelopeFollower offsetSmooth {100.f, 100.f};
    };
}