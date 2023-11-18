//
// Created by August Pemberton on 25/09/2023.
//

#include "AuthorizationManager.h"

AuthorizationManager::AuthorizationManager()
: properties(Resources::getConfigFile())
{
    loadSavedAuth();
}

// ============================ Authorization

bool AuthorizationManager::isAuthorized() {
#if JUCE_DEBUG
    return true;
#endif

#ifdef SKIP_AUTH
    return true;
#endif
    return isAuthorizedCache;
}

bool AuthorizationManager::tryAuth(juce::String serial) {
    if (isSerialValid(serial)) {
        saveSerial(serial);
        return true;
    }

    return false;
}

void AuthorizationManager::saveSerial(juce::String serial) {
    properties->reload();
    properties->setValue("serial", serial);

    properties->removeValue("demoStarted");
    properties->removeValue("demoStartTime");
    properties->save();

    loadSavedAuth();
}

void AuthorizationManager::loadSavedAuth() {

    // if we are in the demo, we are authorized
    if (hasDemoStarted() && !hasDemoFinished()) {
        isAuthorizedCache = true;
        return;
    }

    auto savedSerial = properties->getValue("serial", "");
    if (savedSerial == "") return;
    if (!isSerialValid(savedSerial)) return;

    isAuthorizedCache = true;
}

bool AuthorizationManager::isSerialValid(juce::String serial) {
    // implement your own logic here!
    return true;
}

// ============================ Demo

void AuthorizationManager::startDemo() {
    if (hasDemoStarted()) return;
    properties->setValue("demoStarted", true);
    properties->setValue("demoStartTime", juce::Time::currentTimeMillis());
    properties->saveIfNeeded();

    loadSavedAuth();
}


bool AuthorizationManager::isDemoActive() {
    return !isAuthorized() && hasDemoStarted() && !hasDemoFinished();
}

bool AuthorizationManager::hasDemoStarted() {
    return properties->getBoolValue("demoStarted", false);
}

bool AuthorizationManager::hasDemoFinished() {
    if (!hasDemoStarted()) return false;
    return getDemoTimeLeft() <= juce::RelativeTime(0);
}

juce::RelativeTime AuthorizationManager::getDemoTimeLeft() {
    if (!hasDemoStarted()) return juce::RelativeTime(0);

    const static auto demoLength = juce::RelativeTime(60*60*24*5);
    auto demoStartTime = juce::Time(properties->getDoubleValue("demoStartTime"));
    auto demoEndTime = demoStartTime + demoLength;

    if (demoEndTime < juce::Time::getCurrentTime()) return juce::RelativeTime(0);
    return demoEndTime - juce::Time::getCurrentTime();
}





