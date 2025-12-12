/*
  ==============================================================================

    PresetFileManager.h
    Created: 22 Jan 2022 6:19:33pm
    Author:  August Pemberton

  ==============================================================================
*/

#pragma once

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

            const auto& configFile = getConfigFile();
            const auto configTimeOpened = configFile->getIntValue("timeOpened", 0);
            configFile->setValue("timeOpened", timeOpened + configTimeOpened);
            configFile->save();
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

    private:

        juce::FileLogger errorLogger;
        std::mutex getConfigFileMutex;
    };
}