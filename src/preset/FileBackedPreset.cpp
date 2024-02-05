//
// Created by August Pemberton on 26/10/2023.
//
#include "FileBackedPreset.h"

#include <utility>
#include "imagiro_processor/src/config/Resources.h"

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

    Preset preset = Preset::fromState(s, validateProcessor);
    return {FileBackedPreset(preset, file)};
}

choc::value::Value FileBackedPreset::getState() const {
    auto presetState = preset.getState();
    presetState.addMember("path", getPresetRelativePath().toStdString());
    presetState.addMember("favorite", Resources::getInstance()->isPresetFavorite(*this));
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

bool FileBackedPreset::getFavorite() { return Resources::getInstance()->isPresetFavorite(*this); }
void FileBackedPreset::setFavorite(bool fav) { Resources::getInstance()->setPresetFavorite(*this, fav); }

juce::String FileBackedPreset::getPresetRelativePath() const {
    return file.getRelativePathFrom(Resources::getInstance()->getPresetsFolder()).toStdString();
}
