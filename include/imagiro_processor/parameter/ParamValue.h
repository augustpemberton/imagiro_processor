// ParamValue.h
#pragma once

#include <nlohmann/json.hpp>

namespace imagiro {

    struct ParamValue {
        float value01{0.f};
        float userValue{0.f};
        float (*toProcessor)(float userValue, double bpm, double sampleRate){nullptr};
    };

    // Serialize userValue for human-readable presets - value01 is derived from range
    inline void to_json(nlohmann::json& j, const ParamValue& v) {
        j = v.userValue;
    }

    inline void from_json(const nlohmann::json& j, ParamValue& v) {
        v.userValue = j.get<float>();
        // value01 needs to be recomputed by ParamController using the param range
    }

} // namespace imagiro
