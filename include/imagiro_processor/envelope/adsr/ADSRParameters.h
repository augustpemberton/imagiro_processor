//
// Created by August Pemberton on 15/01/2024.
//

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <nlohmann/json.hpp>

namespace imagiro {

class ADSRParameters : public juce::ADSR::Parameters {
public:
    using juce::ADSR::Parameters::Parameters;
    bool operator==(const ADSRParameters& other) const {
        return attack == other.attack
        && decay == other.decay
        && sustain == other.sustain
        && release == other.release;
    }
};

inline void to_json(nlohmann::json& j, const ADSRParameters& p) {
    j = nlohmann::json{
        {"attack", p.attack},
        {"decay", p.decay},
        {"sustain", p.sustain},
        {"release", p.release}
    };
}

inline void from_json(const nlohmann::json& j, ADSRParameters& p) {
    p.attack = j.value("attack", 0.01f);
    p.decay = j.value("decay", 0.4f);
    p.sustain = j.value("sustain", 1.f);
    p.release = j.value("release", 0.4f);
}

} // namespace imagiro
