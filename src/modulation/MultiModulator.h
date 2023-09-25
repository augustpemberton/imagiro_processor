//
// Created by August Pemberton on 30/09/2022.
//

#pragma once

namespace imagiro {
    class MultiModulator : public Modulator {
    public:
        MultiModulator(std::unique_ptr<Modulator> initialModulator, const juce::String& defaultMacroName = "macro")
        : Modulator(imagiro::Modulator::Type::LFO) {

            nextModulator = std::move(initialModulator);
            updateModulator();

            state.setProperty("macroName", defaultMacroName, nullptr);
        }

        struct Listener { virtual void modulatorChanged() {} };
        void addListener(Listener* l) { listeners.add(l); }
        void removeListener(Listener* l) { listeners.remove(l); }

        void setModulator(std::unique_ptr<Modulator> m) {
            nextModulator = std::move(m);
        }

        void setSampleRate(double sr) override {
            Modulator::setSampleRate(sr);
            if (currentModulator)
                currentModulator->setSampleRate(sr);
        }

        float getValue() override { return currentModulator->getValue(); }

        void prepareParameters() override {
            if (currentModulator)
                currentModulator->prepareParameters();
        }

        void updateModulator() {
            currentModulator = std::move(nextModulator);
            nextModulator.reset();
            currentModulator->setSampleRate(sampleRate);
            listeners.call(&Listener::modulatorChanged);
            DBG("set modulator to " + Modulator::typeNames[(int)currentModulator->getType()]);
            currentModulator->loadState(state);
            prepareParameters();
        }

        void advanceSample(juce::AudioSampleBuffer* b, int sampleIndex) override {
            if (nextModulator) updateModulator();

            currentModulator->advanceSample(b, sampleIndex);
        }

        Type getType() override { return currentModulator->getType(); }

        std::unique_ptr<Modulator>& getCurrentModulator() { return currentModulator; }

        juce::ValueTree getState() override {
            if (currentModulator)
                return currentModulator->getState();
            return juce::ValueTree("modState");
        }

        void loadState(juce::ValueTree newState) override {
            // copy properties from new state
            for (auto p=0; p<newState.getNumProperties(); p++) {
                auto newPropName = newState.getPropertyName(p);
                state.setProperty(newPropName,
                                  newState.getProperty(newPropName), nullptr);
            }

            // TODO: copy children? but overwrite when same state
            if (currentModulator) currentModulator->loadState(state);
        }

        bool isDefaultBipolar() const override {
            if (!currentModulator) return false;
            return currentModulator->isDefaultBipolar();
        }

    private:
        juce::ValueTree state {"modState"};
        std::unique_ptr<Modulator> currentModulator {nullptr};
        std::unique_ptr<Modulator> nextModulator {nullptr};
        juce::ListenerList<Listener> listeners;
    };
}


