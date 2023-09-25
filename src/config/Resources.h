/*
  ==============================================================================

    PresetFileManager.h
    Created: 22 Jan 2022 6:19:33pm
    Author:  August Pemberton

  ==============================================================================
*/

#pragma once

#include "../preset/Preset.h"

class Resources {

public:
    static juce::File getDataFolder() {

        auto dataFolder = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
#if JUCE_MAC
                .getChildFile ("Application Support")
#endif
                .getChildFile ("imagiro").getChildFile (JucePlugin_Name);

        if (!dataFolder.exists())
            dataFolder.createDirectory();

        return dataFolder;
    }

    static juce::File getSampleSetFolder(juce::String productName = JucePlugin_Name) {
        auto prop = getConfigFile();

        // Default path
        auto defaultPath = getDefaultSampleSetFolder(productName).getFullPathName();
        if (!prop->containsKey("samplepath")) {
            prop->setValue("samplepath", defaultPath);
            prop->saveIfNeeded();
        }

        // If installer set a valid custom path, consume and delete
        auto installerSamplesFile = getDataFolder().getChildFile("samplepath.txt");
        if (installerSamplesFile.exists()) {
            auto f = juce::File(installerSamplesFile.loadFileAsString());
            installerSamplesFile.deleteFile();

            if (f.getNumberOfChildFiles(juce::File::findFiles, "*.imag") > 0)
                prop->setValue("samplepath", f.getFullPathName());
        }

        prop->saveIfNeeded();
        return juce::File(getConfigFile()->getValue("samplepath", defaultPath));
    }

    static juce::Array<juce::File> getSampleSetFiles(juce::String productName = JucePlugin_Name) {
        return getSampleSetFolder(std::move(productName))
            .findChildFiles(juce::File::findFiles, true, "*.imag");
    }

    static std::unique_ptr<juce::PropertiesFile> getConfigFile() {
        juce::PropertiesFile::Options options;
        //options.storageFormat = juce::PropertiesFile::storeAsBinary;

        auto configFile = getDataFolder().getChildFile("config");
        if (configFile.exists()) configFile.create();

        return std::make_unique<juce::PropertiesFile>(configFile, options);
    }

    static juce::File getSerialFile() {
        return getDataFolder().getChildFile("imagiro.serial");
    }

    static juce::File getPresetsFolder() {
        auto folder = getDataFolder().getChildFile("Presets");
        if (!folder.exists()) folder.createDirectory();
        return folder;
    }

    static std::map<juce::String, std::vector<Preset>> reloadPresetsMap() {
        std::map<juce::String, std::vector<Preset>> presetsMap;
        auto categories = getPresetsFolder().findChildFiles(juce::File::findDirectories, false);
        categories.sort();
        for (const auto& folder: categories) {
            auto ps = folder.findChildFiles(juce::File::findFiles, false, "*.impreset");
            std::vector<Preset> categoryPresets;
            for (const auto& p: ps) {
                auto preset = Preset::createFromFile(p);
                if (preset.has_value())
                    categoryPresets.push_back(*preset);
            }

            std::sort(categoryPresets.begin(), categoryPresets.end(), [&](Preset& a, Preset& b) {
                return a.getName() < b.getName();
            });

            presetsMap[folder.getFileNameWithoutExtension()] = categoryPresets;
        }
        return presetsMap;
    }

    std::map<juce::String, std::vector<Preset>> cachedPresetsMap;

    std::map<juce::String, std::vector<Preset>> getPresets(bool reload = false) {
        if (reload || cachedPresetsMap.empty()) cachedPresetsMap = reloadPresetsMap();
        return cachedPresetsMap;
    }

    std::vector<Preset> getPresetsList() {
        std::vector<Preset> presetsList;
        auto categories = getPresets();
        for (const auto& category : categories) {
            for (const auto& preset : category.second) {
                presetsList.push_back(preset);
            }
        }

        return presetsList;
    }

    Preset getNextPreset(juce::File lastLoadedPresetFile) {
        auto list = getPresetsList();
        if (list.empty()) return {};
        auto it = std::find_if(list.begin(), list.end(), [&](const Preset& preset) {
            return preset.getPresetFile() == lastLoadedPresetFile;
        });
        if (it == list.end()-1 || it == list.end()) {
            return list.front();
        }
        return *(it+1);
    }

    Preset getPrevPreset(juce::File lastLoadedPresetFile) {
        auto list = getPresetsList();
        if (list.empty()) return {};
        auto it = std::find_if(list.begin(), list.end(), [&](const Preset& preset) {
            return preset.getPresetFile() == lastLoadedPresetFile;
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

    static bool isPresetFavorite(Preset& p) {
        auto favoritesTree = getFavoritesTree();

        return std::any_of(
                favoritesTree.begin(),
                favoritesTree.end(),
                [&](auto fav) {
                    return getPresetsFolder().getChildFile(
                            fav.getProperty("path", "").toString()) == p.getPresetFile();
        });
    }

    static void setPresetFavorite(Preset& p, bool favorite = true) {
        auto favoriteTree = getFavoritesTree();

        favoriteTree = (favorite ? addPresetFavorite(favoriteTree, p)
                          : removePresetFavorite(favoriteTree, p));

        getConfigFile()->setValue("favorites", favoriteTree.createXml().get());
    }

private:
    static juce::ValueTree addPresetFavorite(juce::ValueTree favoriteTree, Preset& p) {
        auto path = p.getPresetFile().getRelativePathFrom(getPresetsFolder());
        auto newTree = juce::ValueTree("favorite")
                .setProperty( "path", path, nullptr);

        favoriteTree.appendChild(newTree, nullptr);
        return favoriteTree;
    }

    static juce::ValueTree removePresetFavorite(juce::ValueTree favoriteTree, Preset& p) {
        auto path = p.getPresetFile().getRelativePathFrom(getPresetsFolder());

        for (auto i=0; i<favoriteTree.getNumChildren(); i++) {
            if (favoriteTree.getChild(i).getProperty("path") == path) {
                favoriteTree.removeChild(i, nullptr);
                break;
            }
        }

        return favoriteTree;
    }

    static juce::File getDefaultSampleSetFolder(juce::String productName) {
#if JUCE_MAC
		return juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
			.getChildFile("Application Support")
			.getChildFile("imagiro").getChildFile(productName)
			.getChildFile("samples");

#elif JUCE_WINDOWS
		return juce::File::getSpecialLocation(juce::File::globalApplicationsDirectory)
			.getChildFile("imagiro").getChildFile(productName)
			.getChildFile("samples");

#else #error "unsupported OS"
#endif

	}
};
