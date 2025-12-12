// ParamValue.h
#pragma once

#include <nlohmann/json.hpp>

namespace imagiro {

    struct ParamValue {
        float value01{0.f};
        float userValue{0.f};
        float (*toProcessor)(float userValue, double bpm, double sampleRate){nullptr};
    };

    // Only serialize value01 - userValue and processorValue are derived
    inline void to_json(nlohmann::json& j, const ParamValue& v) {
        j = v.value01;
    }

    inline void from_json(const nlohmann::json& j, ParamValue& v) {
        v.value01 = j.get<float>();
        // userValue and processorValue need to be recomputed by ParamController
    }

} // namespace imagiro
