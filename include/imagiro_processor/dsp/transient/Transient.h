//
// Created by August Pemberton on 09/09/2023.
//

#pragma once
#include <choc/containers/choc_Value.h>

struct Transient {
    Transient(int sample, float db, bool enabled = true) : transientSample(sample), transientDb(db) {}
    int transientSample {0};
    float transientDb {0};
    bool enabled {true};

    [[nodiscard]] choc::value::Value getState() const {
        auto v = choc::value::createObject("Transient");
        v.addMember("transientSample", transientSample);
        v.addMember("transientDb", transientDb);
        v.addMember("enabled", enabled);
        return v;
    }

    static Transient fromState(const choc::value::ValueView& state) {
        return { state["transientSample"].getWithDefault(0),
                 state["transientDb"].getWithDefault(0.f),
                 state["enabled"].getWithDefault(true) };
    }

};
