//
// Created by August Pemberton on 08/05/2025.
//

#pragma once
#include "Parameter.h"

using namespace imagiro;
class ProxyParameter : public Parameter, Parameter::Listener {
public:
    ProxyParameter(const std::string &uid, const std::string &name = "Proxy Parameter", bool isInternal = false, bool isAutomatable = true)
        : Parameter(uid, name, {{}}, isInternal, true, isAutomatable)
    { }

    ~ProxyParameter() override {
        if (proxyTarget != nullptr) proxyTarget.load()->removeListener(this);
    }

    bool isProxySet() const { return proxyTarget != nullptr || queuedProxyTarget != nullptr; }
    void clearProxyTarget() {
        proxyTarget = nullptr;
        queuedProxyTarget = nullptr;
    }

    void queueProxyTarget(Parameter& p) {
        queuedProxyTarget = &p;
    }

    void processQueuedTarget() {
        if (queuedProxyTarget) {
            setProxyTarget(*queuedProxyTarget);
            queuedProxyTarget = nullptr;
        }
    }

    void setProxyTarget(Parameter& p) {
        if (proxyTarget) {
            proxyTarget.load()->removeListener(this);
            if (proxyOriginalModTarget) {
                proxyTarget.load()->setModTarget(*proxyOriginalModTarget);
                proxyOriginalModTarget.reset();
            }
        }
        internal = p.isInternal();
        proxyTarget = &p;
        p.addListener(this);

        proxyOriginalModTarget = p.getModTarget();
        p.setModTarget(modTarget);

        asyncConfigChangedFlag = true;
        asyncValueUpdateFlag = true;

        listeners.call(&Listener::parameterChangedSync, this);
        listeners.call(&Listener::configChangedSync, this);
    }

    void OnTargetUpdated() override {
        Parameter::OnTargetUpdated();
        // if (!proxyTarget) return;

        // proxyTarget.load()->setValue(getModValue());
    }

    // ModTarget& getModTarget() override {
    //     return proxyTarget ? proxyTarget.load()->getModTarget() : modTarget;
    // }

    // setters
    void setValue(const float newValue) override {
        if (proxyTarget) proxyTarget.load()->setValue(newValue);
        Parameter::setValue(newValue);
    }

    void setValueAndNotifyHost(float f, bool forceUpdate) override {
        if (proxyTarget) proxyTarget.load()->setValueAndNotifyHost(f, forceUpdate);
        Parameter::setValueAndNotifyHost(f, forceUpdate);
    }

    // getters
    float getValue() const override {
        return proxyTarget ? proxyTarget.load()->getValue() : value01.load();
    }

    std::vector<ParameterConfig> getAllConfigs() const override {
        return proxyTarget ? proxyTarget.load()->getAllConfigs() : configs;
    }

    ParameterConfig *getConfig() override {
        return proxyTarget ? proxyTarget.load()->getConfig() : &configs[configIndex];
    }

    const ParameterConfig *getConfig() const override {
        return proxyTarget ? proxyTarget.load()->getConfig() : &configs[configIndex];
    }

    int getConfigIndex() override {
        return proxyTarget ? proxyTarget.load()->getConfigIndex() : configIndex;
    }

    void callValueChangedListeners() override {
        if (proxyTarget) proxyTarget.load()->callValueChangedListeners();
        Parameter::callValueChangedListeners();
    }

    void setConfig(const int index) override {
        if (proxyTarget) proxyTarget.load()->setConfig(index);
        else Parameter::setConfig(index);
    }

    // forward listener events
    void parameterChanged (Parameter*) override { listeners.call(&Listener::parameterChanged, this); }
    void parameterChangedSync (Parameter*) override { listeners.call(&Listener::parameterChangedSync, this); }
    void configChanged(Parameter*) override { listeners.call(&Listener::configChanged, this); }
    void gestureStarted(Parameter*) override { listeners.call(&Listener::gestureStarted, this); }
    void gestureStartedSync(Parameter*) override { listeners.call(&Listener::gestureStartedSync, this); }
    void gestureEnded(Parameter*) override { listeners.call(&Listener::gestureEnded, this); }
    void gestureEndedSync(Parameter*) override { listeners.call(&Listener::gestureEndedSync, this); }
    void lockChanged(Parameter*) override { listeners.call(&Listener::lockChanged, this); }

    void setLocked(const bool locked) override {
        if (proxyTarget) proxyTarget.load()->setLocked(locked);
        else Parameter::setLocked(locked);
    }

    bool isLocked() const override {
        return proxyTarget ? proxyTarget.load()->isLocked() : Parameter::isLocked();
    }

    std::string getUserValueText() const override {
        return proxyTarget ? proxyTarget.load()->getUserValueText() : "";
    }

    juce::String getText(float normalisedValue, int) const override {
        return proxyTarget ? proxyTarget.load()->getText(normalisedValue, 0) : "0";
    }

    float getJitterAmount() const override {
        return proxyTarget ? proxyTarget.load()->getJitterAmount() : jitterAmount;
    }

    juce::String getName(int maximumStringLength) const override {
        return proxyTarget ? proxyTarget.load()->getName(maximumStringLength) : name;
    }

private:
    std::atomic<Parameter*> proxyTarget {nullptr};
    std::atomic<Parameter*> queuedProxyTarget{nullptr};

    std::optional<ModTarget> proxyOriginalModTarget;
};
