//
// Created by August Pemberton on 26/10/2023.
//

#pragma once
#include "Preset.h"

class FileBackedPreset {
public:
    FileBackedPreset();
    FileBackedPreset (Preset  p, juce::File file);
    choc::value::Value getState() const;

    static std::optional<FileBackedPreset> createFromFile(const juce::File& file);
    void saveToFile(juce::File f);
    void save();

    static FileBackedPreset save (Preset p, const std::string& category);

    juce::File getFile() const;
    juce::String getPresetRelativePath() const;

    bool getFavorite();
    void setFavorite(bool fav);

    const Preset& getPreset() const { return preset; }

private:
    Preset preset;
    juce::File file;
};


