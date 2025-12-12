//
// Created by August Pemberton on 15/01/2024.
//

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

    class ADSRParameters : public juce::ADSR::Parameters {
    public:
        using juce::ADSR::Parameters::Parameters;

        [[nodiscard]] choc::value::Value getState() const {
            auto state = choc::value::createObject("ADSRParams");
            state.addMember("attack", attack);
            state.addMember("decay", decay);
            state.addMember("sustain", sustain);
            state.addMember("release", release);
            return state;
        }

        static ADSRParameters fromState(const choc::value::ValueView& state) {
            ADSRParameters p;
            p.attack = state["attack"].getWithDefault(0.01f);
            p.decay = state["decay"].getWithDefault(0.4f);
            p.sustain = state["sustain"].getWithDefault(1.f);
            p.release = state["release"].getWithDefault(0.4f);
            return p;
        }
    };