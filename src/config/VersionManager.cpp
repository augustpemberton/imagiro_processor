//
// Created by August Pemberton on 25/09/2023.
//

#include "VersionManager.h"

VersionManager::VersionManager(juce::String currentVersion_, juce::String pluginSlug_)
        : juce::Thread("update check"),
          currentVersion(currentVersion_), pluginSlug(pluginSlug_)
{
    startTimer(2000); // delay update check for VST3 manifest
}

VersionManager::~VersionManager() {
    stopThread(500);
}

void VersionManager::timerCallback() {
    stopTimer();
    checkForUpdate();
}

std::optional<juce::String> VersionManager::isUpdateAvailable() {
    if (updateAvailable) return newVersion;
    return {};
}

juce::String VersionManager::getCurrentVersion() {
    return currentVersion;
}

void VersionManager::checkForUpdate() {
    startThread();
}

std::optional<juce::String> VersionManager::fetchLatestVersion() {
    DBG("checking for updates...");
    auto updateURL = juce::URL(updateBaseURL + "/api/products/version/" + pluginSlug);
    auto productInfo = updateURL.readEntireTextStream();
    if (productInfo == "") return {};
    auto productJSON = juce::JSON::fromString(updateURL.readEntireTextStream());

    auto versionKey = "version";
#if BETA
    versionKey = "beta_version";
#endif

    return productJSON.getProperty(versionKey, 0).toString();
}

bool VersionManager::checkForUpdateInternal() {
    newVersion = fetchLatestVersion();
    if (!newVersion) return false;

    auto newVersionArr = juce::StringArray::fromTokens(*newVersion, ".", "\"");
    auto currentVersionArr = juce::StringArray::fromTokens(currentVersion, ".", "\"");

    for (int i=0; i<3; i++) {
        if (newVersionArr.size()-1 < i) return false;
        if (currentVersionArr.size()-1 < i) return true;

        if (newVersionArr[i].getIntValue() > currentVersionArr[i].getIntValue()) return true;
        if (newVersionArr[i].getIntValue() < currentVersionArr[i].getIntValue()) return false;
    }

    return false;
}

void VersionManager::run() {
    // Wait before checking so we don't check during plugin scan
    wait(100);
    if (threadShouldExit()) return;

    updateAvailable = checkForUpdateInternal();
    if (updateAvailable) listeners.call(&Listener::OnUpdateDiscovered);
}

juce::String VersionManager::getUpdateURL() {
    return updateBaseURL + "/api/download/slug/" + pluginSlug;
}


