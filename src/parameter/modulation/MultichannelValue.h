//
// Created by August Pemberton on 28/05/2025.
//

#pragma once
#include <array>
#include <set>

#include "choc/containers/choc_Value.h"
#include "imagiro_util/src/util.h"


template <unsigned int NumChannels>
class MultichannelValue {
public:
    void setGlobalValue(float value) { globalValue = value; }
    float getGlobalValue() const { return globalValue; }

    void resetValue() {
        voiceValues.fill(0);
        globalValue = 0;
        alteredVoices.clear();
    }

    void setVoiceValue(size_t voiceIndex, float value) {
        voiceValues[voiceIndex] = value;

        if (!imagiro::almostEqual(value, 0.f)) {
            alteredVoices.insert(voiceIndex);
        } else {
            alteredVoices.erase(voiceIndex);
        }
    }
    float getVoiceValue(size_t voiceIndex) { return voiceValues[voiceIndex]; }

    const std::set<size_t> getAlteredVoices() const { return alteredVoices; }
    const std::set<size_t>& getVoiceValues() const { return voiceValues; }

    choc::value::Value getState() const {
        auto v = choc::value::createObject("MultichannelValue");

        v.addMember("global", globalValue);

        for (const auto index : alteredVoices) {
            v.addMember(std::to_string(index), voiceValues[index]);
        }

        v.addMember("values", v);

        return v;
    }

private:
    float globalValue {0};
    std::array<float, NumChannels> voiceValues {0};
    std::set<size_t> alteredVoices {};
};
