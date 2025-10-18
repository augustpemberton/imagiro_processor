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

#ifndef RESOURCE_NAME
    #ifdef RESOURCE_NAME
        #define RESOURCE_NAME RESOURCE_NAME
    #elif defined(JucePlugin_Name)
        #define RESOURCE_NAME JucePlugin_Name
    #else
        #define RESOURCE_NAME "myplugin"
    #endif
#endif

#ifndef COMPANY_NAME
#ifdef JucePlugin_Manufacturer
        #define COMPANY_NAME JucePlugin_Manufacturer
    #else
        #define COMPANY_NAME "mycompany"
    #endif
#endif

namespace imagiro {
    class Resources {

    public:
        juce::int64 lastOpened;

        Resources()
            : errorLogger(getDataFolder().getChildFile("error-log.txt"), "======= log started ========")
        {
            lastOpened = juce::Time::currentTimeMillis();
        }

        ~Resources() {
            auto timeOpened = juce::Time::currentTimeMillis() - lastOpened;

            auto configTimeOpened = getConfigFile()->getIntValue("timeOpened", 0);
            getConfigFile()->setValue("timeOpened", timeOpened + configTimeOpened);
            getConfigFile()->save();
        }

        juce::FileLogger& getErrorLogger() {
            return errorLogger;
        }

        static juce::File getDataFolder() {

            auto dataFolder = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
    #if JUCE_MAC
                    .getChildFile ("Application Support")
    #endif
                    .getChildFile (COMPANY_NAME)
                            .getChildFile (RESOURCE_NAME);

            if (!dataFolder.exists())
                dataFolder.createDirectory();

            return dataFolder;
        }

        static juce::File getSystemDataFolder() {

            auto dataFolder = juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory)
    #if JUCE_MAC
                    .getChildFile ("Application Support")
    #endif
                    .getChildFile (COMPANY_NAME)
                    .getChildFile (RESOURCE_NAME);

            if (!dataFolder.exists())
                dataFolder.createDirectory();

            return dataFolder;
        }

        std::unique_ptr<juce::PropertiesFile> propertiesFile;
        std::unique_ptr<juce::PropertiesFile>& getConfigFile() {
            std::lock_guard g (getConfigFileMutex);
            if (!propertiesFile) {
                juce::PropertiesFile::Options options;
                //options.storageFormat = juce::PropertiesFile::storeAsBinary;

                auto configFile = getDataFolder().getChildFile("config");
                if (configFile.exists()) configFile.create();

                propertiesFile = std::make_unique<juce::PropertiesFile>(configFile, options);
            }

            propertiesFile->reload(); // make sure to read the latest contents first
            return propertiesFile;
        }

        juce::File presetsFolder;
        juce::File getPresetsFolder() {
            if (!presetsFolder.exists()) {
                auto folder = getDataFolder().getChildFile("Presets");
                if (!folder.exists()) folder.createDirectory();
                presetsFolder = folder;
            }
            return presetsFolder;
        }

        void reloadPresets(imagiro::Processor* validateProcessor = nullptr) {
            reloadPresetsMap(validateProcessor);
        }

        void reloadPresetsMap(imagiro::Processor* validateProcessor = nullptr) {
            std::map<juce::String, std::vector<FileBackedPreset>> presetsMap;
            auto categories = getPresetsFolder().findChildFiles(juce::File::findDirectories, false);
            categories.sort();
            for (const auto& folder: categories) {
                auto ps = folder.findChildFiles(juce::File::findFiles, false, "*.impreset");
                std::vector<FileBackedPreset> categoryPresets;
                for (const auto& p: ps) {
                    auto preset = FileBackedPreset::createFromFile(p, validateProcessor);
                    if (preset.has_value())
                        categoryPresets.push_back(*preset);
                }

                std::sort(categoryPresets.begin(), categoryPresets.end(), [&](FileBackedPreset& a, FileBackedPreset& b) {
                    return a.getPreset().getName() < b.getPreset().getName();
                });

                presetsMap[folder.getFileNameWithoutExtension()] = categoryPresets;
            }
            cachedPresetsMap = presetsMap;

            reloadPresetsList();
        }

        std::map<juce::String, std::vector<FileBackedPreset>> cachedPresetsMap;

        std::map<juce::String, std::vector<FileBackedPreset>> getPresets(bool reload = false) {
            if (reload || cachedPresetsMap.empty()) reloadPresetsMap();
            return cachedPresetsMap;
        }

        std::vector<FileBackedPreset> cachedPresetsList;

        void reloadPresetsList() {
            std::vector<FileBackedPreset> presetsList;
            auto categories = cachedPresetsMap;
            for (const auto& category : categories) {
                for (const auto& preset : category.second) {
                    if (preset.getPreset().isAvailable()) presetsList.push_back(preset);
                }
            }

            cachedPresetsList = presetsList;
        }

        std::vector<FileBackedPreset> getPresetsList(bool reload = false) {
            if (cachedPresetsList.empty() || reload) reloadPresetsList();
            return cachedPresetsList;
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

        std::optional<juce::ValueTree> favoritesTree;
        juce::ValueTree getFavoritesTree() {
            if (!favoritesTree.has_value()) {
                auto favorites = std::make_unique<juce::XmlElement>("favorites");
                if (getConfigFile()->containsKey("favorites")) {
                    favorites = getConfigFile()->getXmlValue("favorites");
                }

                favoritesTree = juce::ValueTree::fromXml(*favorites);
            }
            return favoritesTree.value();
        }

        bool isPresetFavorite(const FileBackedPreset& p) {
            auto favTree = getFavoritesTree();

            return std::any_of(
                    favTree.begin(),
                    favTree.end(),
                    [&](auto fav) {
                        return getPresetsFolder().getChildFile(
                                fav.getProperty("path", "").toString()) == p.getFile();
            });
        }

        void setPresetFavorite(FileBackedPreset& p, bool favorite = true) {
            auto favoriteTree = getFavoritesTree();

            favoriteTree = (favorite ? addPresetFavorite(favoriteTree, p)
                              : removePresetFavorite(favoriteTree, p));

            getConfigFile()->setValue("favorites", favoriteTree.createXml().get());
        }

    private:
        juce::ValueTree addPresetFavorite(juce::ValueTree favoriteTree, FileBackedPreset& p) {
            auto path = p.getFile().getRelativePathFrom(getPresetsFolder());
            auto newTree = juce::ValueTree("favorite")
                    .setProperty( "path", path, nullptr);

            favoriteTree.appendChild(newTree, nullptr);
            return favoriteTree;
        }

        juce::ValueTree removePresetFavorite(juce::ValueTree favoriteTree, FileBackedPreset& p) {
            auto path = p.getFile().getRelativePathFrom(getPresetsFolder());

            for (auto i=0; i<favoriteTree.getNumChildren(); i++) {
                if (favoriteTree.getChild(i).getProperty("path") == path) {
                    favoriteTree.removeChild(i, nullptr);
                    break;
                }
            }

            return favoriteTree;
        }

        juce::FileLogger errorLogger;
        std::mutex getConfigFileMutex;
    };
}