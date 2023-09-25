//
// Created by August Pemberton on 22/04/2022.
//

#include "Preset.h"
#include "../config/Resources.h"

Preset::Preset()
        :   presetTree("preset"),
            presetFilePath(presetTree, "presetFile", nullptr),
            name(presetTree, "name", nullptr),
            author(presetTree, "author", nullptr),
            favorite(presetTree, "favorite", nullptr),
            validPreset(false) {
    presetTree.setProperty(Constants::PresetProperties::name, "init", nullptr);
    presetTree.appendChild(juce::ValueTree("parameters"), nullptr);
}

Preset::Preset(juce::ValueTree& tree)
        :   presetTree(tree),
            presetFilePath(presetTree, "presetFile", nullptr),
            name(presetTree, "name", nullptr),
            author(presetTree, "author", nullptr),
            favorite(presetTree, "favorite", nullptr),
            validPreset(true) {
}

Preset::Preset(const Preset &other)
        :   presetTree("preset"),
            presetFilePath(presetTree, "presetFile", nullptr),
            name(presetTree, "name", nullptr),
            author(presetTree, "author", nullptr),
            favorite(presetTree, "favorite", nullptr),
            validPreset(other.validPreset)
{
    presetTree.copyPropertiesAndChildrenFrom(other.presetTree, nullptr);
}

Preset& Preset::operator=(const Preset& other) noexcept {
    presetTree.copyPropertiesAndChildrenFrom(other.presetTree, nullptr);
    validPreset = other.isValid();

    return *this;
}

Preset& Preset::operator=(Preset&& other) noexcept {
    if (this != &other) {
        presetTree = other.presetTree;
        name.referTo(presetTree, "name", nullptr);
        author.referTo(presetTree, "author", nullptr);
        favorite.referTo(presetTree, "favorite", nullptr);
        presetFilePath.referTo(presetTree, "presetFile", nullptr);
        validPreset = other.validPreset;

        other.presetTree = juce::ValueTree();
        other.name.referTo(other.presetTree, "name", nullptr);
        other.author.referTo(other.presetTree, "author", nullptr);
        other.favorite.referTo(other.presetTree, "favorite", nullptr);
        other.presetFilePath.referTo(other.presetTree, "presetFile", nullptr);
        other.validPreset = false;
    }

    return *this;
}

std::optional<Preset> Preset::createFromFile(const juce::File& file) {
    auto xml = juce::parseXML(file);
    if (!xml) return {};
    auto tree = juce::ValueTree::fromXml(*xml);
    if (!tree.isValid()) return {};
    auto p = Preset(tree);
    p.presetFilePath = file.getFullPathName();
    return {p};
}

void Preset::addParamState(imagiro::Parameter::ParamState state) {
    getParameterTree().appendChild(state.toTree(), nullptr);
}

std::vector<imagiro::Parameter::ParamState> Preset::getParamStates() const {
    std::vector<imagiro::Parameter::ParamState> states;
    if (!getParameterTree().isValid()) return states;
    for (auto stateTree : getParameterTree())
        states.push_back(imagiro::Parameter::ParamState::fromTree(stateTree));
    return states;
}

juce::ValueTree Preset::getParameterTree() {
    return presetTree.getOrCreateChildWithName("parameters", nullptr);
}

juce::ValueTree Preset::getParameterTree() const {
    return presetTree.getChildWithName("parameters");
}

bool Preset::getFavorite() {
   return Resources::isPresetFavorite(*this);
}
void Preset::setFavorite(bool fav) {
    Resources::setPresetFavorite(*this, fav);
}
