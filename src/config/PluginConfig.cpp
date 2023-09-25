/*
  ==============================================================================

    PluginConfig.cpp
    Created: 30 Jan 2022 9:52:37pm
    Author:  pembe

  ==============================================================================
*/

#include "shared_plugin_helpers/shared_plugin_helpers.h"
#include "PluginConfig.h"

PluginConfig::PluginConfig(bool checkForUpdate) : juce::Thread("UpdateCheckThread") {
    properties = Resources::getConfigFile();

    if (checkForUpdate)
        checkUpdate();

    isAuthorizedCache = getAuth();
}

bool PluginConfig::isPluginAuthorized(bool recheck)
{
    if (recheck) isAuthorizedCache = getAuth();
    return isAuthorizedCache;
}

bool PluginConfig::getAuth() {
    if (isSerialValid(properties->getValue("serial"))) return true;

    if (hasDemoStarted() && !hasDemoFinished()) return true;

    isAuthorizedCache = false;
    return false;
}

bool PluginConfig::validateSerial(juce::String serial)
{
	if (isSerialValid(serial)) {
		saveSerial(serial);
		return true;
	}

	return false;
}

void PluginConfig::saveSerial(juce::String serial)
{
	if (!isSerialValid(serial)) {
		jassertfalse;
		return;
	}

	properties->reload();
	properties->setValue("serial", serial);
    properties->removeValue("demoStarted");
    properties->removeValue("demoStartTime");
	properties->saveIfNeeded();

    isAuthorizedCache = getAuth();
}

bool PluginConfig::isSerialValid(juce::String serial)
{
	if (serial.length() != 16) return false;

	auto n = 0;
	n += serial[0] - '0';
	n += serial[2] - '0';
	n += serial[7] - '0';
	n += serial[15] - '0';
	n %= serial[12] - '0';

    auto serialRemainder = 2;
#ifdef SERIAL_REMAINDER
    serialRemainder = SERIAL_REMAINDER;
#endif

	return n == serialRemainder;
}

void PluginConfig::checkUpdate()
{
    if (hasCheckedForUpdate) return;
    if (!isThreadRunning())
        startThread();
}

bool PluginConfig::checkForUpdate() {
    DBG("checking for updates...");
    auto updateURL = juce::URL(updateBaseURL + "/api/products/version/" + juce::String(PROJECT_ID));
    DBG(updateURL.toString(true));
    auto productInfo = updateURL.readEntireTextStream();
    DBG(productInfo);
    hasCheckedForUpdate = true;
    if (productInfo == "") return false;
    auto productJSON = juce::JSON::fromString(updateURL.readEntireTextStream());

    auto versionKey = "version";
#if BETA
    versionKey = "beta_version";
#endif
    newVersion = productJSON.getProperty(versionKey, 0).toString();
    auto currentVersion = PROJECT_VERSION;

    auto newVersionArr = juce::StringArray::fromTokens(newVersion, ".", "\"");
    auto currentVersionArr = juce::StringArray::fromTokens(currentVersion, ".", "\"");

    for (int i=0; i<3; i++) {
        if (newVersionArr.size()-1 < i) return false;
        if (currentVersionArr.size()-1 < i) return true;

        if (newVersionArr[i].getIntValue() > currentVersionArr[i].getIntValue()) return true;
        if (newVersionArr[i].getIntValue() < currentVersionArr[i].getIntValue()) return false;
    }
   
    return false;
}

void PluginConfig::run() {
    // Wait before checking so we don't check during plugin scan
    wait(100);
    if (threadShouldExit()) return;
    isUpdateAvailable = checkForUpdate();
    sendChangeMessage();
}

void PluginConfig::startDemo() {
    if (hasDemoStarted()) return;
    properties->setValue("demoStarted", true);
    properties->setValue("demoStartTime", juce::Time::currentTimeMillis());
    properties->saveIfNeeded();
}

bool PluginConfig::hasDemoStarted() {
    return properties->getBoolValue("demoStarted", false);
}

bool PluginConfig::hasDemoFinished() {
    return getDemoTimeLeft() < juce::RelativeTime(0);
}

juce::RelativeTime PluginConfig::getDemoTimeLeft() {
    if (!hasDemoStarted()) return juce::RelativeTime(0);

    const static auto demoLength = juce::RelativeTime(60*60*24*5);
    auto demoStartTime = juce::Time(properties->getDoubleValue("demoStartTime"));
    auto demoEndTime = demoStartTime + demoLength;

    if (demoEndTime < juce::Time::getCurrentTime()) return juce::RelativeTime(0);
    return demoEndTime - juce::Time::getCurrentTime();
}

