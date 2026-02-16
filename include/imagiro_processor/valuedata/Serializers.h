//
// Created by August Pemberton on 14/07/2025.
//

#pragma once
#include <nlohmann/json.hpp>
#include "imagiro_util/src/structures/beman/inplace_vector.h"
#include "juce_core/juce_core.h"

#include "Serialize.h"

template<>
struct Serializer<juce::File> {
    static json serialize(const juce::File& f) {
        return json(f.getFullPathName().toStdString());
    }

    static juce::File load(const json& state) {
        juce::File file;
        try {
            file = juce::File(state.get<std::string>());
        } catch (...) {
            jassertfalse;
        }
        return file;
    }
};
