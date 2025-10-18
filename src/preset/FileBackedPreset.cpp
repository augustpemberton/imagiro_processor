//
// Created by August Pemberton on 26/10/2023.
//
#include "FileBackedPreset.h"

#include <utility>
#include "imagiro_processor/src/config/Resources.h"
#include <choc/text/choc_JSON.h>

namespace imagiro {
    FileBackedPreset::FileBackedPreset() {

    }

    FileBackedPreset::FileBackedPreset(Preset  p, juce::File f)
            : preset(std::move(p)), file(std::move(f))
    {
    }

    juce::File FileBackedPreset::getFile() const {
        return file;
    }

    std::optional<FileBackedPreset> FileBackedPreset::createFromFile(const juce::File &file, imagiro::Processor* validateProcessor) {
        auto presetString= file.loadFileAsString();
        if (presetString.isEmpty()) return {};
        auto s = choc::json::parse(presetString.toStdString());

        Preset preset = Preset::fromState(s, false, validateProcessor);
        return {FileBackedPreset(preset, file)};
    }

    choc::value::Value FileBackedPreset::getState() const {
        auto presetState = preset.getState();
        return presetState;
    }

    choc::value::Value FileBackedPreset::getUIState() const {
        const juce::SharedResourcePointer<Resources> resources;

        auto presetState = preset.getUIState();
        presetState.addMember("path", getPresetRelativePath().toStdString());
        presetState.addMember("favorite", resources->isPresetFavorite(*this));
        return presetState;
    }

    void FileBackedPreset::saveToFile(juce::File f) {
        auto s = choc::json::toString(getState());
        if (!f.exists()) f.create();
        f.replaceWithText(s);
    }

    void FileBackedPreset::save() {
        saveToFile(file);
    }

    FileBackedPreset FileBackedPreset::save(Preset p, const std::string& category) {
        juce::SharedResourcePointer<Resources> resources;
        auto categoryFolder = resources->getPresetsFolder().getChildFile(category);
        if (!categoryFolder.exists()) categoryFolder.createDirectory();
        auto file = categoryFolder.getChildFile(p.getName() + juce::String(PRESET_EXT).toStdString());
        file.create();

        FileBackedPreset fbp (p, file);
        fbp.save();
        juce::SharedResourcePointer<Resources>()->reloadPresets();
        return fbp;
    }

    bool FileBackedPreset::getFavorite() const {
        const juce::SharedResourcePointer<Resources> resources;
        return resources->isPresetFavorite(*this);
    }
    void FileBackedPreset::setFavorite(bool fav) {
        const juce::SharedResourcePointer<Resources> resources;
        resources->setPresetFavorite(*this, fav);
    }

    juce::String FileBackedPreset::getPresetRelativePath() const {
        const juce::SharedResourcePointer<Resources> resources;
        return file.getRelativePathFrom(resources->getPresetsFolder()).toStdString();
    }
}