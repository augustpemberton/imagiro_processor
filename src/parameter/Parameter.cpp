//
// Created by August Pemberton on 12/09/2022.
//

#include "Parameter.h"

#include <utility>
#include "ParameterConfig.h"

namespace imagiro {

    Parameter::Parameter(juce::String uid, juce::String name,
                         ParameterConfig config, bool internal,
                         bool automatable, int versionHint)

            : juce::RangedAudioParameter({uid, versionHint}, name,
                                         juce::AudioProcessorParameterWithIDAttributes()
                                                 .withAutomatable (automatable)),
              configs({config}),
              internal(internal),
              uid(uid),
              name(name)
    {
        this->value01 = convertTo0to1(this->getConfig()->defaultValue);
        startTimerHz(20);
    }

    Parameter::Parameter(juce::String uid, juce::String name,
                         std::vector<ParameterConfig> configs, bool internal,
                         bool automatable, int versionHint)

            : juce::RangedAudioParameter({uid, versionHint}, name,
                                         juce::AudioProcessorParameterWithIDAttributes()
                                                 .withAutomatable (automatable)),
              configs(std::move(configs)),
              internal(internal),
              uid(uid),
              name(name)
    {
        this->value01 = convertTo0to1(this->getConfig()->defaultValue);
    }

    Parameter::~Parameter() noexcept {
        // sync param will be created after, therefore destroyed first
        // so cannot remove listener as it will already be destroyed at this point
    }

    bool Parameter::isToggle() {
        return getConfig()->range.start == 0 && getConfig()->range.end == getConfig()->range.interval;
    }

    float Parameter::getModVal(bool useConversion) {
        // TODO: implement mod matrix
//        if (modMatrix) return modMatrix->getValue(this, useConversion);

        if (useConversion) return getVal();
        return getUserValue();
    }

    float Parameter::getVal(int stepsToAdvance) {
        if (getConfig()->conversionFunction) return getConfig()->conversionFunction(getUserValue());
        return getUserValue();
    }

    float Parameter::getUserValue() const {
        return convertFrom0to1(value01);
    }

    int Parameter::getUserValueInt() const {
        return (int) getUserValue();
    }

    bool Parameter::getUserValueBool() const {
        return getUserValue() != 0.f;
    }

    float Parameter::getUserDefaultValue() const {
        return getConfig()->range.getRange().clipValue(getConfig()->defaultValue);
    }

    void Parameter::setUserValue(float v) {
        v = getConfig()->range.snapToLegalValue(getConfig()->range.getRange().clipValue(v));
        if (!almostEqual(convertFrom0to1(value01), v)) {
            value01 = convertTo0to1(v);
            sendUpdateFlag = true;
            valueChanged();
        }
    }

    void Parameter::setUserValueNotifingHost (float v, bool force) {
        v = getConfig()->range.snapToLegalValue(getConfig()->range.getRange().clipValue(v));
        if (! almostEqual (value01, convertTo0to1(v)) || force) {
            value01 = convertTo0to1(v);
            if (!internal)
                setValueNotifyingHost (getValue());

            sendUpdateFlag = true;
            valueChanged();
        }
    }

    void Parameter::setUserValueAsUserAction (float f) {
        beginUserAction();

        if (internal) {
            auto v = getConfig()->range.snapToLegalValue(getConfig()->range.getRange().clipValue(f));
            value01 = convertTo0to1(v);
            setValue(f);
            sendUpdateFlag = true;
            valueChanged();
        } else {
            setUserValueNotifingHost(f);
        }

        endUserAction();
    }

    juce::String Parameter::getUserValueText() const {
        return getText (getValue(), 1000) + suffix;
    }

    juce::String Parameter::userValueToText (float val) {
        return getText (getConfig()->range.convertTo0to1 (val), 1000) + suffix;
    }

    DisplayValue Parameter::getDisplayValue() const {
        return getDisplayValueForUserValue(getUserValue());
    }

    DisplayValue Parameter::getDisplayValueForUserValue(float userValue) const {
        if (getConfig()->textFunction) {
            return getConfig()->textFunction (*this, userValue);
        }
        return {getUserValueText()};
    }

    void Parameter::beginUserAction() {
        if (!internal) {
            currentUserActionsCount++;
            if (currentUserActionsCount == 1) {
                //beginChangeGesture();
                listeners.call(&Parameter::Listener::gestureStarted, this);
            }
        }
    }

    void Parameter::endUserAction() {
        if (!internal) {
            currentUserActionsCount--;
            if (currentUserActionsCount == 0) {
                //endChangeGesture();
                listeners.call(&Parameter::Listener::gestureEnded, this);
            }
        }
    }

    void Parameter::addListener (Listener* listener) {
        listeners.add (listener);
    }

    void Parameter::removeListener (Listener* listener) {
        listeners.remove (listener);
    }

    void Parameter::timerCallback() {
        if (sendUpdateFlag) {
            listeners.call (&Listener::parameterChanged, this);
            sendUpdateFlag = false;
        }
    }

    void Parameter::valueChanged() {
        listeners.call (&Listener::parameterChangedSync, this);
    }

    Parameter::ParamState Parameter::getState() {
        return {uid, getUserValue(), configs[configIndex].name, locked};
    }

    void Parameter::setState (const ParamState& state) {
        jassert (state.uid == uid);

        for (auto c=0; c<configs.size(); c++) {
            if (configs[c].name == state.config) {
                setConfig(c);
            }
        }

        setLocked(state.locked);

        setUserValueNotifingHost(state.value);
    }

    float Parameter::getValue() const {
        return juce::jlimit (0.0f, 1.0f,value01);
    }

    void Parameter::setValue (float valueIn) {
        valueIn = juce::jlimit (0.0f, 1.0f, valueIn);
        float newValue = getConfig()->range.snapToLegalValue (getConfig()->range.convertFrom0to1 (valueIn));

        if (!almostEqual (value01, convertTo0to1(newValue))) {
            value01 = convertTo0to1(newValue);
            sendUpdateFlag = true;
            valueChanged();
        }

    }

    float Parameter::getDefaultValue() const {
        return getConfig()->range.convertTo0to1 (getConfig()->defaultValue);
    }

    juce::String Parameter::getName (int maximumStringLength) const {
        return name.substring (0, maximumStringLength);
    }

    juce::String Parameter::getLabel() const {
        return suffix;
    }

    int Parameter::getNumSteps() const {
        if (getConfig()->range.interval == 0) return 100;
        return juce::roundToInt ((getConfig()->range.end -getConfig()->range.start) /getConfig()->range.interval);
    }

    juce::String Parameter::getText (float val, int /*maximumStringLength*/) const
    {
        if (getConfig()->textFunction)
            return getConfig()->textFunction (*this,getConfig()->range.convertFrom0to1 (val)).withSuffix();

        auto uv =getConfig()->range.snapToLegalValue (getConfig()->range.convertFrom0to1 (val));
        return formatNumber (uv);
    }

    float Parameter::getValueForText (const juce::String& text) const {
        if (getConfig()->valueFunction) return getConfig()->valueFunction(*this, text);
        return getConfig()->range.convertTo0to1 (text.getFloatValue());

    }

    bool Parameter::isOrientationInverted() const { return false; }
    bool Parameter::isAutomatable() const { return true; }
    bool Parameter::isMetaParameter() const { return false; }

    const juce::NormalisableRange<float> &Parameter::getNormalisableRange() const {
        return getConfig()->range;
    }

    juce::NormalisableRange<float> Parameter::getUserRange() const {
        return getConfig()->range;
    }

    float Parameter::convertTo0to1 (float v) const {
        return getConfig()->range.convertTo0to1(
                getConfig()->range.getRange().clipValue(v));
    }
    float Parameter::convertFrom0to1 (float v) const {
        return getConfig()->range.snapToLegalValue(getConfig()->range.convertFrom0to1(v));
    }

    int Parameter::getConfigIndex() {
        return configIndex;
    }

    const ParameterConfig* Parameter::getConfig() const {
        return &configs[configIndex];
    }

    ParameterConfig* Parameter::getConfig() {
        return &configs[configIndex];
    }

    void Parameter::setConfig(int index) {
        auto uv = getUserValue();

        index %= configs.size();

        configIndex = index;

        listeners.call(&Listener::rangeChanged, this);
        setUserValueNotifingHost(uv, true);
    }

    void Parameter::setLocked(bool l) {
        locked = l;
    }

    bool Parameter::isLocked() const {
        return locked;
    }

    void Parameter::generateSmoothedValueBlock(int samples) {
        valueSmoother.setTargetValue(getValue());
        for (auto s=0; s<samples; s++) {
            smoothedValueBuffer.setSample(0, s, valueSmoother.getNextValue());
        }
    }

    float Parameter::getSmoothedValue(int blockIndex) {
        return smoothedValueBuffer.getSample(0, blockIndex);
    }

    float Parameter::getSmoothedUserValue(int blockIndex) {
        return convertFrom0to1(getSmoothedValue(blockIndex));
    }

    void Parameter::prepareToPlay(double sampleRate, int samplesPerBlock) {
        valueSmoother.reset(sampleRate, 0.01);
        smoothedValueBuffer.setSize(1, samplesPerBlock);
        smoothedValueBuffer.clear();
    }
}
