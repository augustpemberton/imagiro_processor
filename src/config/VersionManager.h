//
// Created by August Pemberton on 25/09/2023.
//

#pragma once

class VersionManager : juce::Thread {
public:
    VersionManager(juce::String currentVersion, juce::String pluginSlug);
    ~VersionManager();

    struct Listener {
        virtual void OnUpdateDiscovered() {};
    };

    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); };

    std::optional<juce::String> isUpdateAvailable();
    void checkForUpdate();

    juce::String getCurrentVersion();
    juce::String getUpdateURL();

private:
    juce::ListenerList<Listener> listeners;

    void run() override;

    std::optional<juce::String> fetchLatestVersion();
    bool checkForUpdateInternal();

    bool updateAvailable {false};
    std::optional<juce::String> newVersion;
    juce::String currentVersion;
    juce::String pluginSlug;

    const juce::String updateBaseURL =
#if JUCE_DEBUG
            "https://imagi.ro";
            //"http://localhost:3000";
#else
            "https://imagi.ro";
#endif
};
