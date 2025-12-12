//
// Created by August Pemberton on 14/07/2025.
//

#pragma once
#include <choc/containers/choc_Value.h>
#include "imagiro_util/src/structures/beman/inplace_vector.h"
#include "juce_core/juce_core.h"

#include "Serialize.h"

template<>
struct Serializer<juce::File> {
    static choc::value::Value serialize(const juce::File& f) {
        return choc::value::Value{f.getFullPathName().toStdString()};
    }

    static juce::File load(const choc::value::ValueView& state) {
        juce::File file;
        try {
            file = juce::File(state.toString());
        } catch (...) {
            jassertfalse;
        }
        return file;
    }
};

template<>
struct Serializer<std::string> {
    static choc::value::Value serialize(const std::string& s) {
        return choc::value::Value{s};
    }

    static std::string load(const choc::value::ValueView& state) {
        std::string s;
        try {
            s = state.toString();
        } catch (...) {
            jassertfalse;
        }
        return s;
    }
};
