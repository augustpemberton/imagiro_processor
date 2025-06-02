//
// Created by August Pemberton on 30/05/2025.
//

#pragma once


struct DisplayValue {
    juce::String value;
    juce::String suffix{""};

    juce::String withSuffix() { return value + suffix; }
    juce::String withoutSuffix() { return value; }
};
