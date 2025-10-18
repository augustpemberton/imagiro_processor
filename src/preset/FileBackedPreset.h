//
// Created by August Pemberton on 26/10/2023.
//

#pragma once
#include "Preset.h"

namespace imagiro {
    class Resources;

    class FileBackedPreset {
    public:
        FileBackedPreset();
        FileBackedPreset (Preset  p, juce::File file);
        choc::value::Value getState() const;
        choc::value::Value getUIState() const;

        static std::optional<FileBackedPreset> createFromFile(const juce::File& file, imagiro::Processor* validateProcessor = nullptr);
        void saveToFile(juce::File f);
        void save();

        static FileBackedPreset save (Preset p, const std::string& category);

        juce::File getFile() const;
        juce::String getPresetRelativePath() const;

        bool getFavorite() const;
        void setFavorite(bool fav);

        const Preset& getPreset() const { return preset; }

    private:
        Preset preset;
        juce::File file;
    };
}