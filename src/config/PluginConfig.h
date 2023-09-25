/*
  ==============================================================================

    PluginConfig.h
    Created: 30 Jan 2022 9:52:37pm
    Author:  pembe

  ==============================================================================
*/

#pragma once

#include <juce_events/juce_events.h>
#include <juce_data_structures/juce_data_structures.h>

#ifndef PROJECT_VERSION
#define PROJECT_VERSION 1.0
#endif

class PluginConfig : juce::Thread, public juce::ChangeBroadcaster {

public:
	PluginConfig(bool checkForUpdate = true);
    ~PluginConfig() override { stopThread(50); }

	bool isPluginAuthorized(bool recheck = false);
    bool getAuth();
	bool validateSerial(juce::String serial);

    bool getIsUpdateAvailable() const { return isUpdateAvailable; }
	void checkUpdate();
    juce::String getNewVersion() { return newVersion; }

    void startDemo();
    bool hasDemoStarted();
    bool hasDemoFinished();
    bool isRunningDemo() { return hasDemoStarted() && !hasDemoFinished(); }
    juce::RelativeTime getDemoTimeLeft();

    const juce::String updateBaseURL =
#if JUCE_DEBUG
            "http://localhost:3000";
#else
    "https://imagi.ro";
#endif
protected:
    std::unique_ptr<juce::PropertiesFile> properties;
    
	virtual bool isSerialValid(juce::String serial);
	void saveSerial(juce::String serial);

    bool checkForUpdate();
    bool hasCheckedForUpdate {false};
    bool isUpdateAvailable {false};
    juce::String newVersion;

    void run() override;

    std::atomic<bool> isAuthorizedCache {false};

};
