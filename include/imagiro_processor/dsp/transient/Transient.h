//
// Created by August Pemberton on 09/09/2023.
//

#pragma once
#include <nlohmann/json.hpp>

struct Transient {
    Transient() = default;
    Transient(int sample, float db, bool enabled = true) : transientSample(sample), transientDb(db), enabled(enabled) {}
    int transientSample {0};
    float transientDb {0};
    bool enabled {true};
};

inline void to_json(nlohmann::json& j, const Transient& t) {
    j = nlohmann::json{
        {"transientSample", t.transientSample},
        {"transientDb", t.transientDb},
        {"enabled", t.enabled}
    };
}

inline void from_json(const nlohmann::json& j, Transient& t) {
    t.transientSample = j.value("transientSample", 0);
    t.transientDb = j.value("transientDb", 0.f);
    t.enabled = j.value("enabled", true);
}
