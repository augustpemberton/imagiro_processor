//
// Created by August Pemberton on 08/05/2025.
//

#pragma once
#include "Parameter.h"

using namespace imagiro;
class ProxyParameter : public Parameter {
public:
    ProxyParameter(const std::string &uid, const std::string &name = "Proxy Parameter", bool isInternal = false, bool isAutomatable = true)
        : Parameter(uid, name, {{}}, isInternal, true, isAutomatable)
    { }

    bool isProxySet() const { return proxyTarget != nullptr; }
    void clearProxyTarget() { proxyTarget = nullptr; }
    void setProxyTarget(Parameter& p) {
        internal = p.isInternal();
        proxyTarget = &p;
        if (modMatrix) {
            p.setModMatrix(*modMatrix);
        }
    }

    ModTarget& getModTarget() override {
        return proxyTarget ? proxyTarget.load()->getModTarget() : modTarget;
    }

    // setters
    void setValue(const float newValue) override {
        if (proxyTarget) proxyTarget.load()->setValue(newValue);
    }

    void setValueAndNotifyHost(float f, bool forceUpdate) override {
        if (proxyTarget) proxyTarget.load()->setValueAndNotifyHost(f, forceUpdate);
    }

    // getters
    float getValue() const override {
        return proxyTarget ? proxyTarget.load()->getValue() : value01.load();
    }

    std::vector<ParameterConfig> getAllConfigs() const override {
        return proxyTarget ? proxyTarget.load()->getAllConfigs() : configs;
    }

    const ParameterConfig *getConfig() const override {
        return proxyTarget ? proxyTarget.load()->getConfig() : &configs[configIndex];
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

};
