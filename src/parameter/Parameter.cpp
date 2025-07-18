//
// Created by August Pemberton on 12/09/2022.
//

#include "Parameter.h"

#include <utility>
#include "ParameterConfig.h"

namespace imagiro {

    Parameter::Parameter(std::string uid, std::string name,
                         std::vector<ParameterConfig> configs,
                         bool internal, bool meta,
                         bool automatable,
                         float jitter,
                         int versionHint)

            : juce::RangedAudioParameter({uid, versionHint}, name,
                                         juce::AudioProcessorParameterWithIDAttributes()
                                                 .withAutomatable (automatable)
                                                 .withMeta(meta)),
              configs(std::move(configs)),
              internal(internal),
              uid(uid),
              name(name),
              modTarget(name),
              jitterAmount(jitter)
    {
        jassert(!this->configs.empty());
        this->value01 = convertTo0to1(this->Parameter::getConfig()->defaultValue);
        this->valueSmoother.setCurrentAndTargetValue(getProcessorValue(-1));
        this->modTarget.addListener(this);
    }

    Parameter::~Parameter() noexcept {
        this->modTarget.removeListener(this);
        // sync param will be created after, therefore destroyed first
        // so cannot remove listener as it will already be destroyed at this point
    }

    void Parameter::setModMatrix(imagiro::ModMatrix &m) {
        modTarget.setModMatrix(m);
        modMatrix = &m;
    }

    bool Parameter::isToggle() {
        return almostEqual(getConfig()->range.start, 0.f) &&
                almostEqual(getConfig()->range.end, getConfig()->range.interval);
    }

    float Parameter::getUserValue() const {
        return convertFrom0to1(getValue());
    }

    float Parameter::getModValue(int voiceIndex) const {
        auto jitter = (imagiro::rand01() * 2 - 1) * jitterAmount;
        auto valueWithJitter = getValue() + jitter;
        if (!modMatrix) return valueWithJitter;
        return modTarget.getModulatedValue(valueWithJitter, voiceIndex);
    }

    float Parameter::getModUserValue(int voiceIndex) const {
        return convertFrom0to1(getModValue(voiceIndex));
    }

    float Parameter::getProcessorValue(int voiceIndex) const {
        if (const auto conversionFunction = getConfig()->processorConversionFunction)
            return conversionFunction(getModUserValue(voiceIndex), this);

        return getModUserValue(voiceIndex);
    }

    bool Parameter::getBoolValue() const {
        return getUserValue() != 0.f;
    }

    float Parameter::getUserDefaultValue() const {
        return getConfig()->range.getRange().clipValue(getConfig()->defaultValue);
    }

    void Parameter::setValue (float newValue01) {
        jassert(newValue01 >= 0.f && newValue01<= 1.f);

        auto userValueValidated = convertFrom0to1(newValue01, true);
        auto val01Validated = convertTo0to1(userValueValidated);

        if (almostEqual (value01.load(), val01Validated)) return;

        value01 = val01Validated;
        valueChanged();
    }

    void Parameter::setUserValue(float userValue) {
        setValue(convertTo0to1(userValue));
    }

    void Parameter::setValueAndNotifyHost (float val01, bool force) {
        if (almostEqual (value01.load(), val01) && !force) return;
        setValue(val01);
        if (!internal) {
            juce::RangedAudioParameter::sendValueChangedMessageToListeners(value01.load());
        }
    }

    void Parameter::setUserValueAndNotifyHost (float userValue, bool force) {
        setValueAndNotifyHost(convertTo0to1(userValue), force);
    }

    void Parameter::setUserValueAsUserAction (float userValue) {
        beginUserAction();
        setUserValueAndNotifyHost(userValue);
        endUserAction();
    }

    std::string Parameter::getUserValueText() const {
        return (getText (getValue(), 1000) + suffix).toStdString();
    }

    std::string Parameter::userValueToText (float userValue) {
        return (getText (getConfig()->range.convertTo0to1 (userValue), 1000) + suffix).toStdString();
    }

    DisplayValue Parameter::getDisplayValue() const {
        return getDisplayValueForUserValue(getUserValue());
    }

    DisplayValue Parameter::getDisplayValueForUserValue(float userValue) const {
        if (getConfig()->textFunction) {
            return getConfig()->textFunction (userValue, this);
        }
        return {getUserValueText()};
    }

    void Parameter::beginUserAction() {
        if (internal) return;
        juce::RangedAudioParameter::beginChangeGesture();
        listeners.call(&Parameter::Listener::gestureStartedSync, this);
        asyncGestureStartUpdateFlag = true;
    }

    void Parameter::endUserAction() {
        if (internal) return;
        juce::RangedAudioParameter::endChangeGesture();
        listeners.call(&Parameter::Listener::gestureEndedSync, this);
        asyncGestureEndUpdateFlag = false;
    }

    void Parameter::addListener (Parameter::Listener* listener) {
        listeners.add (listener);
    }

    void Parameter::removeListener (Parameter::Listener* listener) {
        listeners.remove (listener);
    }

    void Parameter::callAsyncListenersIfNeeded() {
        if (asyncValueUpdateFlag) {
            asyncValueUpdateFlag = false;
            listeners.call (&Listener::parameterChanged, this);
        }

        if (asyncConfigChangedFlag) {
            asyncConfigChangedFlag = false;
            listeners.call (&Listener::configChanged, this);
        }

        if (asyncGestureStartUpdateFlag) {
            asyncGestureStartUpdateFlag = false;
            listeners.call (&Listener::gestureStarted, this);
        }

        if (asyncGestureEndUpdateFlag) {
            asyncGestureEndUpdateFlag = false;
            listeners.call (&Listener::gestureEnded, this);
        }
    }

    void Parameter::valueChanged() {
        asyncValueUpdateFlag = true;
        listeners.call (&Listener::parameterChangedSync, this);
    }

    Parameter::ParamState Parameter::getState() {
        return {uid, getUserValue(), configs[configIndex].name, jitterAmount, locked};
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
        setJitterAmount(state.jitter);
    }

    float Parameter::getValue() const {
        return juce::jlimit(0.f, 1.f, value01.load());
    }

    float Parameter::getDefaultValue() const {
        return getConfig()->range.convertTo0to1 (getConfig()->defaultValue);
    }

    void Parameter::setName(const juce::String &n) {
        this->name = n.toStdString();
        asyncConfigChangedFlag = true;
        listeners.call(&Listener::configChangedSync, this);
    }

    juce::String Parameter::getName (int maximumStringLength) const {
        return juce::String(name).substring (0, maximumStringLength);
    }

    juce::String Parameter::getLabel() const {
        return suffix;
    }

    int Parameter::getNumSteps() const {
        if (almostEqual(getConfig()->range.interval, 0.f)) return 100;
        auto steps = juce::roundToInt (getConfig()->range.getRange().getLength() / getConfig()->range.interval);
        return steps+1;
    }

    bool Parameter::isDiscrete() const {
        return getConfig()->discrete;
    }

    juce::String Parameter::getText (float val, int /*maximumStringLength*/) const
    {
        if (getConfig()->textFunction) {
            auto userValueValidated = convertFrom0to1(val, true);
            auto val01Validated = convertTo0to1(userValueValidated);
            auto displayVal = getConfig()->textFunction (getConfig()->range.convertFrom0to1 (val01Validated), this);
            return displayVal.withSuffix();
        }

        auto userValueValidated = convertFrom0to1(val, true);
        return formatNumber(userValueValidated);
    }

    float Parameter::getValueForText (const juce::String& text) const {
        if (getConfig()->valueFunction) return getConfig()->valueFunction(text, this);
        return getConfig()->range.convertTo0to1 (text.getFloatValue());
    }

    bool Parameter::isOrientationInverted() const { return false; }

    const juce::NormalisableRange<float> &Parameter::getNormalisableRange() const {
        return getConfig()->range;
    }

    float Parameter::convertTo0to1 (float v) const {
        return getConfig()->range.convertTo0to1(getConfig()->range.getRange().clipValue(v));
    }

    float Parameter::convertFrom0to1 (float v, bool snapToLegalValue) const {
        v = getConfig()->range.convertFrom0to1(v);

        if (snapToLegalValue) v = getConfig()->range.snapToLegalValue(v);
        return v;
    }

    int Parameter::getConfigIndex() {
        return configIndex;
    }

    ParameterConfig* Parameter::getConfig() {
        return &configs[configIndex];
    }

    const ParameterConfig* Parameter::getConfig() const {
        return &configs[configIndex];
    }

    void Parameter::setConfig(int index) {
        if (index == static_cast<int>(configIndex)) return;

        auto uv = getUserValue();
        configIndex = index % configs.size();

        listeners.call(&Listener::configChangedSync, this);
        asyncConfigChangedFlag = true;
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
        if (smootherNeedsUpdate) {
            valueSmoother.reset(sampleRate, smoothTimeSeconds);
            smootherNeedsUpdate = false;
        }

        auto target = getProcessorValue();
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
       if (!hasGeneratedSmoothBufferThisBlock) {
           valueSmoother.skip(samplesThisBlock);
       }

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

    void Parameter::callValueChangedListeners() {
        valueChanged();
    }

    void Parameter::OnTargetUpdated() {
        valueSmoother.setTargetValue(getProcessorValue(-1));
        callValueChangedListeners();
    }

}
