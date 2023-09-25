//
// Created by August Pemberton on 27/11/2022.
//

#pragma once
#include "Modulator.h"

namespace imagiro {
    class MacroModulator : public Modulator {
    public:
        MacroModulator(Processor& p, Parameter* val, int mID, bool useHistory)
                : Modulator(Modulator::Type::Macro, false, useHistory),
                  processor(p), valueParam(val), macroID(mID)
        {
        }

        void setSampleRate(double sr) override {
            smoothValue.setSampleRate(sr);
        }

        void advanceSample(juce::AudioSampleBuffer *b, int sampleIndex) override {
            smoothValue.pushSample(valueParam->cached(), false);
        }

        float getValue() override {
            return smoothValue.getEnvelope();
        }

        void prepareParameters() override {
            valueParam->updateCache();
        }

        juce::String getMacroPropertyName() {
            return "macroName" + juce::String(macroID);
        }

        juce::String getMacroName() {
            return processor.lastLoadedPreset->getTree().getProperty(
                    getMacroPropertyName());
        }

        void setMacroName(const juce::String& name) {
            if (processor.lastLoadedPreset) {
                processor.lastLoadedPreset->getTree().setProperty(
                        getMacroPropertyName(), name, nullptr);
            }
        }

        juce::ValueTree getState() override {
            auto tree = juce::ValueTree("modState");
            tree.setProperty("macroName", getMacroName(), nullptr);
            return tree;
        }

        void loadState(juce::ValueTree state) override {
            setMacroName(state.getProperty("macroName",
                                           "macro " + juce::String(macroID)));
        }

    private:
        Processor& processor;
        Parameter* valueParam;

        EnvelopeFollower smoothValue {10, 10};

        int macroID;
    };
}
