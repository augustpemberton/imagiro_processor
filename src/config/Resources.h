/*
  ==============================================================================

    PresetFileManager.h
    Created: 22 Jan 2022 6:19:33pm
    Author:  August Pemberton

  ==============================================================================
*/

#pragma once

#include "../preset/Preset.h"
#include "imagiro_processor/src/preset/FileBackedPreset.h"

#ifndef PROJECT_NAME
    #ifdef JucePlugin_Name
        #define PROJECT_NAME JucePlugin_Name
    #else
        #define PROJECT_NAME "myplugin"
    #endif
#endif

#ifndef COMPANY_NAME
#ifdef JucePlugin_Manufacturer
        #define COMPANY_NAME JucePlugin_Manufacturer
    #else
        #define COMPANY_NAME "mycompany"
    #endif
#endif

class Resources {

public:
    static juce::File getDataFolder() {

        auto dataFolder = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
#if JUCE_MAC
                .getChildFile ("Application Support")
#endif
                .getChildFile (COMPANY_NAME)
                        .getChildFile (PROJECT_NAME);

        if (!dataFolder.exists())
            dataFolder.createDirectory();

        return dataFolder;
    }

    static juce::File getSytemDataFolder() {

        auto dataFolder = juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory)
#if JUCE_MAC
                .getChildFile ("Application Support")
#endif
                .getChildFile (COMPANY_NAME)
                .getChildFile (PROJECT_NAME);

        if (!dataFolder.exists())
            dataFolder.createDirectory();

        return dataFolder;
    }

    static std::unique_ptr<juce::PropertiesFile> getConfigFile() {
        juce::PropertiesFile::Options options;
        //options.storageFormat = juce::PropertiesFile::storeAsBinary;

        auto configFile = getDataFolder().getChildFile("config");
        if (configFile.exists()) configFile.create();

        return std::make_unique<juce::PropertiesFile>(configFile, options);
    }

    static juce::File getPresetsFolder() {
        auto folder = getDataFolder().getChildFile("Presets");
        if (!folder.exists()) folder.createDirectory();
        return folder;
    }

    void reloadPresetsMap() {
        std::map<juce::String, std::vector<FileBackedPreset>> presetsMap;
        auto categories = getPresetsFolder().findChildFiles(juce::File::findDirectories, false);
        categories.sort();
        for (const auto& folder: categories) {
            auto ps = folder.findChildFiles(juce::File::findFiles, false, "*.impreset");
            std::vector<FileBackedPreset> categoryPresets;
            for (const auto& p: ps) {
                auto preset = FileBackedPreset::createFromFile(p);
                if (preset.has_value())
                    categoryPresets.push_back(*preset);
            }

            std::sort(categoryPresets.begin(), categoryPresets.end(), [&](FileBackedPreset& a, FileBackedPreset& b) {
                return a.getPreset().getName() < b.getPreset().getName();
            });

            presetsMap[folder.getFileNameWithoutExtension()] = categoryPresets;
        }
        cachedPresetsMap = presetsMap;
    }

    std::map<juce::String, std::vector<FileBackedPreset>> cachedPresetsMap;

    std::map<juce::String, std::vector<FileBackedPreset>> getPresets(bool reload = false) {
        if (reload || cachedPresetsMap.empty()) reloadPresetsMap();
        return cachedPresetsMap;
    }

    std::vector<FileBackedPreset> cachedPresetsList;
    std::vector<FileBackedPreset> getPresetsList(bool reload = false) {
        if (!cachedPresetsList.empty() && !reload) return cachedPresetsList;
        std::vector<FileBackedPreset> presetsList;
        auto categories = getPresets();
        for (const auto& category : categories) {
            for (const auto& preset : category.second) {
                presetsList.push_back(preset);
            }
        }

        cachedPresetsList = presetsList;
        return presetsList;
    }

    FileBackedPreset getNextPreset(juce::File lastLoadedPresetFile) {
        auto list = getPresetsList();
        if (list.empty()) return {};
        auto it = std::find_if(list.begin(), list.end(), [&](const FileBackedPreset& preset) {
            return preset.getFile() == lastLoadedPresetFile;
        });
        if (it == list.end()-1 || it == list.end()) {
            return list.front();
        }
        return *(it+1);
    }

    FileBackedPreset getPrevPreset(juce::File lastLoadedPresetFile) {
        auto list = getPresetsList();
        if (list.empty()) return {};
        auto it = std::find_if(list.begin(), list.end(), [&](const FileBackedPreset& preset) {
            return preset.getFile() == lastLoadedPresetFile;
        });
        if (it == list.end()) return list.front();
        if (it == list.begin()) return list.back();
        return *(it-1);
    }

    static juce::ValueTree getFavoritesTree() {
        auto favorites = std::make_unique<juce::XmlElement>("favorites");
        if (getConfigFile()->containsKey("favorites")) {
            favorites = getConfigFile()->getXmlValue("favorites");
        }

        return juce::ValueTree::fromXml(*favorites);

    }

    static bool isPresetFavorite(const FileBackedPreset& p) {
        auto favoritesTree = getFavoritesTree();

        return std::any_of(
                favoritesTree.begin(),
                favoritesTree.end(),
                [&](auto fav) {
                    return getPresetsFolder().getChildFile(
                            fav.getProperty("path", "").toString()) == p.getFile();
        });
    }

    static void setPresetFavorite(FileBackedPreset& p, bool favorite = true) {
        auto favoriteTree = getFavoritesTree();

        favoriteTree = (favorite ? addPresetFavorite(favoriteTree, p)
                          : removePresetFavorite(favoriteTree, p));

        getConfigFile()->setValue("favorites", favoriteTree.createXml().get());
    }

private:
    static juce::ValueTree addPresetFavorite(juce::ValueTree favoriteTree, FileBackedPreset& p) {
        auto path = p.getFile().getRelativePathFrom(getPresetsFolder());
        auto newTree = juce::ValueTree("favorite")
                .setProperty( "path", path, nullptr);

        favoriteTree.appendChild(newTree, nullptr);
        return favoriteTree;
    }

    static juce::ValueTree removePresetFavorite(juce::ValueTree favoriteTree, FileBackedPreset& p) {
        auto path = p.getFile().getRelativePathFrom(getPresetsFolder());

        for (auto i=0; i<favoriteTree.getNumChildren(); i++) {
            if (favoriteTree.getChild(i).getProperty("path") == path) {
                favoriteTree.removeChild(i, nullptr);
                break;
            }
        }

        return favoriteTree;
    }
};
