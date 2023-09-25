//
// Created by August Pemberton on 22/09/2022.
//

#pragma once
#include "../structures/CircularFIFO.h"
#include "imagiro_gui/src/waveform2/WaveformHistory.h"

namespace imagiro {
    class Modulator {
    public:
        enum class Type { LFO, Env, Macro, Pulse, Rand};
        inline static juce::StringArray typeNames { "lfo", "env", "macro", "pulse", "rand" };
        inline static juce::Array<Modulator::Type> types {
                imagiro::Modulator::Type::LFO,
                imagiro::Modulator::Type::Env,
                imagiro::Modulator::Type::Macro,
                imagiro::Modulator::Type::Pulse,
                imagiro::Modulator::Type::Rand,
        };

        Modulator(Type type, bool defaultBipolar = false, bool useHistory = true)
                : bipolar(defaultBipolar), modType(type) {
            if (useHistory) history = std::make_unique<WaveformHistory>(48000);
        }

        virtual ~Modulator() = default;

        /**
         * Returns a value between 0 and 1.
         */
        virtual float getValue() = 0;

        virtual void advance(int samples, juce::AudioSampleBuffer* b) {
            prepareParameters();
            for (auto i=0; i<samples; i++) {
                advanceSample(b, i);
                addToHistory(getValue());
            }
        }

        virtual void prepareParameters() {}

        virtual void advanceSample(juce::AudioSampleBuffer *b, int sampleIndex) {}

        void addToHistory(float val) {
            if (history) history->pushSample(val);
        }

        virtual void setSampleRate(double sr) {
            this->sampleRate = sr;
            if (history) history->setSampleRate(sr);
        }

        WaveformHistory* getHistory() {
            return history ? history.get() : nullptr;
        }

        ModSrcId id;
        virtual Type getType() { return modType; }

        virtual juce::ValueTree getState() {
            return juce::ValueTree("modState");
        }
        virtual void loadState(juce::ValueTree state) {}

        virtual bool isDefaultBipolar() const { return bipolar; }

    protected:
        bool bipolar;
        Type modType;
        float downsampleAmount {900};
        int downsampleCounter {0};
        float currentMax {0};
        std::unique_ptr<WaveformHistory> history;
        double sampleRate { 44100 };
    };
}
