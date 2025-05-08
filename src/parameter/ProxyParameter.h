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
    }

    // setters
    void setValue(const float newValue) override {
        if (proxyTarget) proxyTarget->setValue(newValue);
    }

    void setValueAndNotifyHost(float f, bool forceUpdate) override {
        if (proxyTarget) proxyTarget->setValueAndNotifyHost(f, forceUpdate);
    }

    // getters
    float getValue() const override {
        return proxyTarget ? proxyTarget->getValue() : value01.load();
    }

    std::vector<ParameterConfig> getAllConfigs() const override {
        return proxyTarget ? proxyTarget->getAllConfigs() : configs;
    }

    const ParameterConfig *getConfig() const override {
        return proxyTarget ? proxyTarget->getConfig() : &configs[configIndex];
    }

    std::string getUserValueText() const override {
        return proxyTarget ? proxyTarget->getUserValueText() : "";
    }

    juce::String getText(float normalisedValue, int) const override {
        return proxyTarget ? proxyTarget->getText(normalisedValue, 0) : "0";
    }

    float getJitterAmount() const override {
        return proxyTarget ? proxyTarget->getJitterAmount() : jitterAmount;
    }

    juce::String getName(int maximumStringLength) const override {
        return proxyTarget ? proxyTarget->getName(maximumStringLength) : name;
    }

private:
    Parameter* proxyTarget {nullptr};

};
