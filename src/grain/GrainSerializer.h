//
// Created by August Pemberton on 14/07/2025.
//

#pragma once
#include "Grain.h"

template<>
struct Serializer<Grain::Serialized> {
    static choc::value::Value serialize(const Grain::Serialized& g) {
        auto state = choc::value::createObject("Grain");
        state.addMember("position", g.position);
        state.addMember("loopFadePosition", g.loopFadePosition);
        state.addMember("loopFadeProgress", g.loopFadeProgress);
        state.addMember("gain", g.gain);
        state.addMember("pitchRatio", g.pitchRatio);
        return state;
    }

    static Grain::Serialized load(const choc::value::ValueView& state) {
        jassertfalse; // this is meant to be one-way
        return Grain::Serialized();
    }
};
