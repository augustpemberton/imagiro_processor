//
// Created by August Pemberton on 22/04/2022.
//

#pragma once
#include <juce_data_structures/juce_data_structures.h>
#include <utility>

class Preset {
public:
    explicit Preset(juce::ValueTree& tree);
    Preset(const Preset& other);
    Preset& operator=(Preset&& other) noexcept;
    Preset& operator=(const Preset& other) noexcept;
    Preset();

    virtual ~Preset() = default;

    static std::optional<Preset> createFromFile(const juce::File& file);
    [[nodiscard]] juce::File getPresetFile() const { return {presetFilePath}; }
    void setPresetFile(const juce::File& f) {presetFilePath = f.getFullPathName();}

    [[nodiscard]] juce::ValueTree getTree() const { return presetTree; }

    void addParamState(imagiro::Parameter::ParamState param);
    [[nodiscard]] std::vector<imagiro::Parameter::ParamState> getParamStates() const;

    [[nodiscard]] bool isValid() const { return validPreset; }

    [[nodiscard]] juce::String getName() const { return name; }
    [[nodiscard]] juce::String getAuthor() const { return author; }
    bool getFavorite();
    void setFavorite(bool fav);

    void setName(const juce::String& name) {this->name = name;}
    void setAuthor(const juce::String& author) {this->author= author;}

    void save() const {
        if (getPresetFile().exists()) {
            getTree().createXml()->writeTo(getPresetFile());
        }
    }

protected:
    juce::ValueTree presetTree;

    juce::CachedValue<juce::String> presetFilePath;
    juce::CachedValue<juce::String> name;
    juce::CachedValue<juce::String> author;
    juce::CachedValue<bool> favorite;

    bool validPreset;
    juce::ValueTree getParameterTree() const;
    juce::ValueTree getParameterTree();

    friend bool operator==(const Preset& rhs, const Preset& lhs) {
        return (rhs.getPresetFile().exists() && rhs.getPresetFile() == lhs.getPresetFile())
        || rhs.getTree().isEquivalentTo(lhs.getTree());
    }

    JUCE_LEAK_DETECTOR (Preset)
};

