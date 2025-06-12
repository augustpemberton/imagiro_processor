//
// Created by August Pemberton on 28/05/2025.
//

#pragma once
#include <array>
#include <set>

#include "ModMatrix.h"
#include "choc/containers/choc_Value.h"
#include "imagiro_util/src/util.h"


template <unsigned int NumChannels>
class MultichannelValue {
public:
    MultichannelValue(bool bipolar = false) : bipolar(bipolar) {}

    void setGlobalValue(float value) { globalValue = value; }
    float getGlobalValue() const { return globalValue; }

    void setBipolar(bool bipolar) { this->bipolar = bipolar; }
    bool getBipolar() const { return this->bipolar; }

    void resetValue() {
        voiceValues.fill(0);
        globalValue = 0;
        alteredVoices.clear();
    }

    void setVoiceValue(float value, size_t voiceIndex) {
        voiceValues[voiceIndex] = value;

        if (!imagiro::almostEqual(value, 0.f)) {
            alteredVoices.insert(voiceIndex);
        } else {
            alteredVoices.erase(voiceIndex);
        }
    }

    float getVoiceValue(size_t voiceIndex) const { return voiceValues[voiceIndex]; }

    const auto& getAlteredVoices() const { return alteredVoices; }
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
    FixedHashSet<size_t, NumChannels> alteredVoices {};
    bool bipolar {false};
};
