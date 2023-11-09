//
// Created by August Pemberton on 25/09/2023.
//

#include "Authorization.h"

Authorization::Authorization()
: properties(Resources::getConfigFile())
{
    loadSavedAuth();
}

// ============================ Authorization

bool Authorization::isAuthorized() {
#if JUCE_DEBUG
    return true;
#endif

#ifdef SKIP_AUTH
    return true;
#endif
    return isAuthorizedCache;
}

bool Authorization::tryAuth(juce::String serial) {
    if (isSerialValid(serial)) {
        saveSerial(serial);
        return true;
    }

    return false;
}

void Authorization::saveSerial(juce::String serial) {
    properties->reload();
    properties->setValue("serial", serial);

    properties->removeValue("demoStarted");
    properties->removeValue("demoStartTime");
    properties->save();

    loadSavedAuth();
}

void Authorization::loadSavedAuth() {

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

bool Authorization::isSerialValid(juce::String serial) {
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

// ============================ Demo

void Authorization::startDemo() {
    if (hasDemoStarted()) return;
    properties->setValue("demoStarted", true);
    properties->setValue("demoStartTime", juce::Time::currentTimeMillis());
    properties->saveIfNeeded();

    loadSavedAuth();
}


bool Authorization::isDemoActive() {
    return !isAuthorized() && hasDemoStarted() && !hasDemoFinished();
}

bool Authorization::hasDemoStarted() {
    return properties->getBoolValue("demoStarted", false);
}

bool Authorization::hasDemoFinished() {
    if (!hasDemoStarted()) return false;
    return getDemoTimeLeft() <= juce::RelativeTime(0);
}

juce::RelativeTime Authorization::getDemoTimeLeft() {
    if (!hasDemoStarted()) return juce::RelativeTime(0);

    const static auto demoLength = juce::RelativeTime(60*60*24*5);
    auto demoStartTime = juce::Time(properties->getDoubleValue("demoStartTime"));
    auto demoEndTime = demoStartTime + demoLength;

    if (demoEndTime < juce::Time::getCurrentTime()) return juce::RelativeTime(0);
    return demoEndTime - juce::Time::getCurrentTime();
}





