//
// Created by August Pemberton on 12/09/2022.
//

#include "Parameter.h"

#include <utility>
#include "ParameterConfig.h"

namespace imagiro {

    Parameter::Parameter(juce::String uid, juce::String name,
                         std::vector<ParameterConfig> configs, bool meta, bool internal,
                         bool automatable, int versionHint)

            : juce::RangedAudioParameter({uid, versionHint}, name,
                                         juce::AudioProcessorParameterWithIDAttributes()
                                                 .withAutomatable (automatable)
                                                 .withMeta(meta)),
              configs(std::move(configs)),
              internal(internal),
              uid(uid),
              name(name)
    {
        this->value01 = convertTo0to1(this->getConfig()->defaultValue);
        juce::RangedAudioParameter::addListener(this);
    }

    Parameter::~Parameter() noexcept {
        // sync param will be created after, therefore destroyed first
        // so cannot remove listener as it will already be destroyed at this point
        juce::RangedAudioParameter::removeListener(this);
    }

    bool Parameter::isToggle() {
        return almostEqual(getConfig()->range.start, 0.f) &&
                almostEqual(getConfig()->range.end, getConfig()->range.interval);
    }

    float Parameter::getUserValue() const {
        return convertFrom0to1(value01);
    }

    float Parameter::getProcessorValue() const {
        if (auto conversionFunction = getConfig()->processorConversionFunction)
            return conversionFunction(value01);

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

    void Parameter::setValue (float valueIn) {
        valueIn = juce::jlimit (0.0f, 1.0f, valueIn);
        float newValue = getNormalisableRange().snapToLegalValue(convertFrom0to1(valueIn, true));

        if (almostEqual (value01.load(), convertTo0to1(newValue))) return;

        value01 = convertTo0to1(newValue);
        sendUpdateFlag = true;
        valueChanged();
    }

    void Parameter::setUserValue(float v) {
        v = getConfig()->range.snapToLegalValue(getConfig()->range.getRange().clipValue(v));

        if (almostEqual(getUserValue(), v)) return;

        value01 = convertTo0to1(v);
        sendUpdateFlag = true;
        valueChanged();
    }

    void Parameter::setValueAndNotifyHost (float v, bool force) {
        if (! almostEqual (value01.load(), v) || force) {
            if (!internal)
                juce::RangedAudioParameter::setValueNotifyingHost (v);
        }
    }

    void Parameter::setUserValueAndNotifyHost (float v, bool force) {
        v = getConfig()->range.snapToLegalValue(getConfig()->range.getRange().clipValue(v));
        if (! almostEqual (value01.load(), convertTo0to1(v)) || force) {
            value01 = convertTo0to1(v);
            if (!internal)
                juce::RangedAudioParameter::setValueNotifyingHost (getValue());

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
            juce::RangedAudioParameter::setValueNotifyingHost(f);
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
        if (internal) return;
        juce::RangedAudioParameter::beginChangeGesture();
        listeners.call(&Parameter::Listener::gestureStartedSync, this);
    }

    void Parameter::endUserAction() {
        if (internal) return;
        juce::RangedAudioParameter::endChangeGesture();
        listeners.call(&Parameter::Listener::gestureEndedSync, this);
    }

    void Parameter::addListener (Parameter::Listener* listener) {
        listeners.add (listener);
    }

    void Parameter::removeListener (Parameter::Listener* listener) {
        listeners.remove (listener);
    }

    /*
        Asynchronous parameter value listener
    */
    void Parameter::parameterValueChanged(int, float) {
        if (!sendUpdateFlag) return;
        listeners.call (&Listener::parameterChanged, this);
        sendUpdateFlag = false;
    }

    /*
        Asynchronous gesture listener
    */
    void Parameter::parameterGestureChanged (int, bool gestureIsStarting) {
        if (gestureIsStarting)
            return listeners.call (&Listener::gestureStarted, this);

        listeners.call (&Listener::gestureEnded, this);
    }

    void Parameter::valueChanged() {
        listeners.call (&Listener::parameterChangedSync, this);
    }

    Parameter::ParamState Parameter::getState() {
        return {uid, getUserValue(), configs[configIndex].name, locked};
    }

    void Parameter::setState (const ParamState& state) {
        jassert (state.uid == uid);
        setUserValueAsUserAction(state.value);

        for (auto c=0; c<configs.size(); c++) {
            if (configs[c].name == state.config) {
                setConfig(c);
            }
        }

        setLocked(state.locked);
    }

    float Parameter::getValue() const {
        return juce::jlimit(0.f, 1.f, value01.load());
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
        auto steps = juce::roundToInt (getConfig()->range.getRange().getLength() / getConfig()->range.interval);
        return steps;
    }

    bool Parameter::isDiscrete() const {
        return getConfig()->discrete;
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

    const juce::NormalisableRange<float> &Parameter::getNormalisableRange() const {
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
        if (index == configIndex) return;

        auto uv = getUserValue();
        index %= configs.size();
        configIndex = index;

        listeners.call(&Listener::configChanged, this);
        setUserValueAndNotifyHost(uv, true);
    }

    void Parameter::setLocked(bool l) {
        locked = l;
        listeners.call(&Listener::lockChanged, this);
    }

    bool Parameter::isLocked() const {
        return locked;
    }

    void Parameter::generateSmoothedValueBuffer(int samples) {
        if (smootherNeedsUpdate) valueSmoother.reset(sampleRate, smoothTimeSeconds);

        auto target = getValue();
        valueSmoother.setTargetValue(target);

        auto blockStart = valueSmoother.getCurrentValue();
        valueSmoother.skip(samples);
        auto blockEnd = valueSmoother.getCurrentValue();
        auto blockInc = (blockEnd - blockStart) / samples;

        auto val = blockStart;
        for (auto s=0; s<samples; s++) {
            smoothedValueBuffer.setSample(0, s, val);
            val += blockInc;
        }
    }

    float Parameter::getSmoothedValue(int blockIndex) {
        if (!hasGeneratedSmoothBufferThisBlock) {
            generateSmoothedValueBuffer(samplesThisBlock);
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
            generateSmoothedValueBuffer(samplesThisBlock);
            hasGeneratedSmoothBufferThisBlock = true;
        }
        return smoothedValueBuffer;
    }
}
