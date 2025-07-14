//
// Created by August Pemberton on 05/02/2025.
//

#pragma once
#include <choc/containers/choc_Value.h>
#define MAX_MOD_CONNECTIONS 1024

struct SerializedMatrixEntry {
    imagiro::SourceID sourceID;
    imagiro::TargetID targetID;
    float depth;
    float attackMS;
    float releaseMS;
    bool bipolar;

    bool operator==(const SerializedMatrixEntry & o) const {
        return o.sourceID == sourceID && o.targetID == targetID;
    }

    choc::value::Value getState() const {
        auto val = choc::value::createObject("ModMatrixEntry");
        val.addMember("sourceID", choc::value::Value(sourceID));
        val.addMember("targetID", choc::value::Value(targetID));
        val.addMember("depth", choc::value::Value(depth));
        val.addMember("attackMS", choc::value::Value(attackMS));
        val.addMember("releaseMS", choc::value::Value(releaseMS));
        val.addMember("bipolar", choc::value::Value(bipolar));
        return val;
    }

    static SerializedMatrixEntry fromState(const choc::value::ValueView& state) {
        SerializedMatrixEntry e {
                state["sourceID"].getWithDefault(0),
                state["targetID"].getWithDefault(0),
                state["depth"].getWithDefault(0.f),
                state["attackMS"].getWithDefault(0.f),
                state["releaseMS"].getWithDefault(0.f),
                state["bipolar"].getWithDefault(false)
        };

        return e;
    }
};

struct SerializedMatrix : public juce::Array<SerializedMatrixEntry> {
    choc::value::Value getState() const {
        auto matrix = choc::value::createEmptyArray();
        for (const auto& entry : *this) {
            matrix.addArrayElement(entry.getState());
        }
        return matrix;
    }

    static SerializedMatrix fromState(const choc::value::ValueView& state) {
        SerializedMatrix m;
        for (const auto& entryState : state) {
            m.add(SerializedMatrixEntry::fromState(entryState));
        }
        return m;
    }
};
