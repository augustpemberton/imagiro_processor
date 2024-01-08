//
// Created by August Pemberton on 12/09/2022.
//

#include "Parameter.h"

#include <utility>
#include "ParameterConfig.h"

namespace imagiro {

    Parameter::Parameter(juce::String uid, juce::String name,
                         ParameterConfig config, bool meta, bool internal,
                         bool automatable, int versionHint)

            : juce::RangedAudioParameter({uid, versionHint}, name,
                                         juce::AudioProcessorParameterWithIDAttributes()
                                                 .withAutomatable (automatable)),
              configs({std::move(config)}),
              isMetaParam(meta),
              internal(internal),
              uid(uid),
              name(name)
    {
        this->value01 = convertTo0to1(this->getConfig()->defaultValue);
        startTimerHz(20);
    }

    Parameter::Parameter(juce::String uid, juce::String name,
                         std::vector<ParameterConfig> configs, bool meta, bool internal,
                         bool automatable, int versionHint)

            : juce::RangedAudioParameter({uid, versionHint}, name,
                                         juce::AudioProcessorParameterWithIDAttributes()
                                                 .withAutomatable (automatable)),
              configs(std::move(configs)),
              isMetaParam(meta),
              internal(internal),
              uid(uid),
              name(name)
    {
        this->value01 = convertTo0to1(this->getConfig()->defaultValue);
        startTimerHz(20);
    }

    Parameter::~Parameter() noexcept {
        // sync param will be created after, therefore destroyed first
        // so cannot remove listener as it will already be destroyed at this point
    }

    bool Parameter::isToggle() {
        return almostEqual(getConfig()->range.start, 0.f) &&
                almostEqual(getConfig()->range.end, getConfig()->range.interval);
    }

    float Parameter::getUserValue() const {
        auto userValue = convertFrom0to1(value01);
        return userValue;
    }

    float Parameter::getProcessorValue() const {
        if (getConfig()->processorConversionFunction)
            return getConfig()->processorConversionFunction(value01);
        else
            return getUserValue();
    }

    float Parameter::getProcessorValue(float userValue) const {
        if (!getConfig()->processorConversionFunction) return userValue;
        else return getConfig()->processorConversionFunction(userValue);
    }

    bool Parameter::getBoolValue() const {
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

    void Parameter::setUserValueNotifyingHost (float v, bool force) {
        v = getConfig()->range.snapToLegalValue(getConfig()->range.getRange().clipValue(v));
        if (! almostEqual (value01.load(), convertTo0to1(v)) || force) {
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
            setUserValueNotifyingHost(f);
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

    void Parameter::addListener (Parameter::Listener* listener) {
        listeners.add (listener);
    }

    void Parameter::removeListener (Parameter::Listener* listener) {
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

        setUserValueNotifyingHost(state.value);
    }

    float Parameter::getValue() const {
        auto v = juce::jlimit(0.f, 1.f, value01.load());
        return v;
    }

    void Parameter::setValue (float valueIn) {
        valueIn = juce::jlimit (0.0f, 1.0f, valueIn);
        float newValue = convertFrom0to1(valueIn, true);

        if (!almostEqual (value01.load(), convertTo0to1(newValue))) {
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
        if (almostEqual(getConfig()->range.interval, 0.f)) return 100;
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
    bool Parameter::isMetaParameter() const { return isMetaParam; }

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

    float Parameter::convertFrom0to1 (float v, bool snapToLegalValue) const {
        v = getConfig()->range.convertFrom0to1(v);

        if (snapToLegalValue) v = getConfig()->range.snapToLegalValue(v);
        return v;
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
        setUserValueNotifyingHost(uv, true);
    }

    void Parameter::setLocked(bool l) {
        locked = l;
    }

    bool Parameter::isLocked() const {
        return locked;
    }

    void Parameter::generateSmoothedProcessorValueBlock(int samples) {
        if (smootherNeedsUpdate) valueSmoother.reset(sampleRate, smoothTimeSeconds);

        auto target = getValue();
        valueSmoother.setTargetValue(target);
        for (auto s=0; s<samples; s++) {
            auto v = valueSmoother.getNextValue();
            auto processorValue = getProcessorValue(convertFrom0to1(v));
            smoothedValueBuffer.setSample(0, s, processorValue);
        }
    }

    float Parameter::getSmoothedProcessorValue(int blockIndex) {
        if (!hasGeneratedSmoothBufferThisBlock) {
            generateSmoothedProcessorValueBlock(samplesThisBlock);
            hasGeneratedSmoothBufferThisBlock = true;
        }
        return smoothedValueBuffer.getSample(0, blockIndex);
    }

    void Parameter::setSmoothTime(float seconds) {
        smoothTimeSeconds = seconds;
        smootherNeedsUpdate = true;
    }

    void Parameter::prepareToPlay(double sr, int samplesPerBlock) {
        sampleRate = sr;
        valueSmoother.reset(sr, smoothTimeSeconds);
        smoothedValueBuffer.setSize(1, samplesPerBlock);
        smoothedValueBuffer.clear();
    }

    void Parameter::startBlock(int samples) {
        samplesThisBlock = samples;
        hasGeneratedSmoothBufferThisBlock = false;
    }

    juce::AudioSampleBuffer &Parameter::getSmoothedProcessorValueBuffer() {
        if (!hasGeneratedSmoothBufferThisBlock) {
            generateSmoothedProcessorValueBlock(samplesThisBlock);
            hasGeneratedSmoothBufferThisBlock = true;
        }
        return smoothedValueBuffer;
    }
}
