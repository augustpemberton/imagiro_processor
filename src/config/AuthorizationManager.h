//
// Created by August Pemberton on 25/09/2023.
//

#pragma once
#include <juce_data_structures/juce_data_structures.h>
#include "Resources.h"

class AuthorizationManager {
public:
    AuthorizationManager();

    struct Listener {
        virtual void onAuthSuccess() {}
    };

    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

    // Auth
    bool isAuthorized(bool ignoreDemo = false);
    bool tryAuth(juce::String serial);

    // Demo
    void startDemo();
    bool hasDemoStarted();
    bool hasDemoFinished();
    bool isDemoActive();
    juce::RelativeTime getDemoTimeLeft();

    juce::String getSerial();

    void logout();

private:
    juce::ListenerList<Listener> listeners;
    void loadSavedAuth();

    bool isSerialValid(juce::String serial);
    void saveSerial(juce::String serial);
    std::atomic<bool> isAuthorizedCache {false};

    juce::SharedResourcePointer<Resources> resources;
    std::unique_ptr<juce::PropertiesFile>& getProperties() { return resources->getConfigFile(); }
};
