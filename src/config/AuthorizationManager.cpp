//
// Created by August Pemberton on 25/09/2023.
//

#include "AuthorizationManager.h"

namespace imagiro {
    AuthorizationManager::AuthorizationManager()
    {
        loadSavedAuth();
    }

    // ============================ Authorization

    bool AuthorizationManager::isAuthorized(bool ignoreDemo) {
        //#if JUCE_DEBUG
        //    return true;
        //#endif

        if (ignoreDemo) {
            auto savedSerial = resources->getConfigFile()->getValue("serial", "");
            if (isSerialValid(savedSerial)) return true;
            return false;
        }

#ifdef SKIP_AUTH
        return true;
#endif
        return isAuthorizedCache;
    }

    bool AuthorizationManager::tryAuth(const juce::String& serial) {
        if (isSerialValid(serial)) {
            saveSerial(serial);
            listeners.call(&Listener::onAuthStateChanged, true);
            return true;
        } else {
            listeners.call(&Listener::onAuthStateChanged, false);
        }

        return false;
    }

    void AuthorizationManager::cancelAuth() {
        isAuthorizedCache = false;
        saveSerial("");
        tryAuth("");
    }

    void AuthorizationManager::saveSerial(const juce::String& serial) {
        const auto& configFile = resources->getConfigFile();
        configFile->reload();
        configFile->setValue("serial", serial);
        configFile->save();

        loadSavedAuth();
    }

    void AuthorizationManager::loadSavedAuth() {

        // if we are in the demo, we are authorized
        if (hasDemoStarted() && !hasDemoFinished()) {
            isAuthorizedCache = true;
            return;
        }

        auto savedSerial = resources->getConfigFile()->getValue("serial", "");
        if (savedSerial == "") return;
        if (!isSerialValid(savedSerial)) return;

        isAuthorizedCache = true;
    }

    bool AuthorizationManager::isSerialValid(juce::String serial) {
        serial = serial.trimCharactersAtEnd("b"); // handle legacy bitbird collab serials
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

    void AuthorizationManager::startDemo() {
        if (hasDemoStarted()) return;
        const auto& configFile = resources->getConfigFile();
        if (!configFile->containsKey("demoStartTime")) {
            configFile->setValue("demoStartTime", juce::Time::currentTimeMillis());
        }
        configFile->setValue("demoStarted", true);
        configFile->saveIfNeeded();

        loadSavedAuth();
    }


    bool AuthorizationManager::isDemoActive() {
        return !isAuthorized() && hasDemoStarted() && !hasDemoFinished();
    }

    bool AuthorizationManager::hasDemoStarted() {
        return resources->getConfigFile()->getBoolValue("demoStarted", false);
    }

    bool AuthorizationManager::hasDemoFinished() {
        auto savedSerial = resources->getConfigFile()->getValue("serial", "");
        if (savedSerial != "" && isSerialValid(savedSerial)) return true;

        if (!hasDemoStarted()) return false;
        return getDemoTimeLeft() <= juce::RelativeTime(0);
    }

    juce::RelativeTime AuthorizationManager::getDemoTimeLeft() {
        if (!hasDemoStarted()) return juce::RelativeTime(0);

        const static auto demoLength = juce::RelativeTime(60*60*24*14);
        auto demoStartTime = juce::Time(resources->getConfigFile()->getDoubleValue("demoStartTime"));
        auto demoEndTime = demoStartTime + demoLength;

        if (demoEndTime < juce::Time::getCurrentTime()) return juce::RelativeTime(0);
        return demoEndTime - juce::Time::getCurrentTime();
    }

    juce::String AuthorizationManager::getSerial() {
        return resources->getConfigFile()->getValue("serial", "");
    }

    void AuthorizationManager::logout() {
        const auto& configFile = resources->getConfigFile();
        configFile->removeValue("serial");
        configFile->save();
        isAuthorizedCache = false;
    }
}