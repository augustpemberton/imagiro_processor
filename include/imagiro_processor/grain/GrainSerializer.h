//
// Created by August Pemberton on 14/07/2025.
//

#pragma once
#include "Grain.h"

template<>
struct Serializer<Grain::Serialized> {
    static json serialize(const Grain::Serialized& g) {
        json state = json::object();
        state["position"] = g.position;
        state["loopFadePosition"] = g.loopFadePosition;
        state["loopFadeProgress"] = g.loopFadeProgress;
        state["gain"] = g.gain;
        state["pitchRatio"] = g.pitchRatio;
        return state;
    }

    static Grain::Serialized load(const json& state) {
        jassertfalse; // this is meant to be one-way
        return Grain::Serialized();
    }
};
