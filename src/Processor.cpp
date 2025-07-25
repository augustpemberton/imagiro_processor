//
// Created by August Pemberton on 12/09/2022.
//

#include "Processor.h"

#include <utility>
#include "choc/text/choc_JSON.h"

namespace imagiro {

    Processor::Processor(juce::String parametersYAMLString, const ParameterLoader& loader, const juce::AudioProcessor::BusesProperties &ioLayouts)
            : ProcessorBase(ioLayouts)
    {
        bypassGain.reset(250);
        mixGain.reset(250);

        bypassGain.setTargetValue(1);
        mixGain.setTargetValue(1);

        juce::AudioProcessor::addListener(this);

        auto parameters = loader.loadParameters(parametersYAMLString, *this);
        for (auto& param : parameters) {
            this->addParam(std::move(param));
        }
        dryBufferLatencyCompensationLine.reset();

        valueData.addListener(this);

        startTimer(0, 8);
    }

    Processor::~Processor() {
        for (auto p : getPluginParameters()) {
            if (p->getUID() == "bypass") p->removeListener(this);
            if (p->getUID() == "mix") p->removeListener(this);
        }
        juce::AudioProcessor::removeListener(this);
        valueData.removeListener(this);
        stopTimer(0);
    }

    void Processor::reset() {
        dryBufferLatencyCompensationLine.reset();
        for (auto p : getPluginParameters())
            p->reset();
    }

    Parameter* Processor::addParam (std::unique_ptr<Parameter> p) {
        p->addListener(this);
        if (p->getUID() == "bypass") {
            bypassGain.setTargetValue(1 - p->getValue());
        }

        if (p->getUID() == "mix") {
            mixGain.setTargetValue(p->getValue());
        }

        p->setModMatrix(modMatrix);

        auto rawPtr = p.get();
        allParameters.add (rawPtr);
        parameterMap[p->getUID()] = rawPtr;
        if (p->isInternal()) internalParameters.add (p.release());
        else addParameter (p.release()->asJUCEParameter());

        parameterListeners.call(&ParameterListener::onParameterAdded, *rawPtr);

        return rawPtr;
    }

    Parameter* Processor::getParameter (const juce::String& uid)
    {
        if (parameterMap.find (uid) != parameterMap.end())
            return parameterMap[uid];

        return nullptr;
    }

    const juce::Array<Parameter*>& Processor::getPluginParameters() { return allParameters; }

    void Processor::timerCallback(const int timerID) {
        if (timerID == 0) {
            for (const auto& p : allParameters) {
                p->callAsyncListenersIfNeeded();
            }
        }
    }

    choc::value::Value Processor::handleMessage(const std::string& type, const choc::value::ValueView& data) {
        if (type == "SetValueData") {
            const auto key = data["key"].getWithDefault("");
            const auto value = data["value"];
            const auto saveInPreset = data["saveInPreset"].getWithDefault(false);

            valueData.set(key, value, saveInPreset);
            return {};
        }

        return OnMessageReceived(type, data);
    }

    void Processor::emitMessage(const std::string &type, choc::value::Value data = {}) {
        messageListeners.call(&MessageListener::OnProcessorMessage, *this, type, data);
    }

    //================================================
    void Processor::addPresetListener(PresetListener *l) { presetListeners.add(l); }
    void Processor::removePresetListener(PresetListener *l) { presetListeners.remove(l); }

    void Processor::queuePreset(const FileBackedPreset& p, bool waitUntilAudioThread) {
        if (!p.getPreset().isAvailable()) return;
        lastLoadedPreset = p;
        queuePreset(p.getPreset(), waitUntilAudioThread);
    }

    void Processor::queuePreset(const Preset& p, bool waitUntilAudioThread) {
        if (!p.isAvailable()) return;
        if (waitUntilAudioThread) nextPreset = std::make_unique<Preset>(p);
        else loadPreset(p);
    }

    std::optional<std::string> Processor::isPresetAvailable(Preset& preset) {
        return {};
    }

    void Processor::getStateInformation(juce::MemoryBlock &destData) {
        auto preset = createPreset(lastLoadedPreset ? lastLoadedPreset->getPreset().getName() : "init", true);
        auto s = preset.getState();

        juce::ValueTree stateTree ("stateTree");
        stateTree.setProperty("state", juce::String(choc::json::toString(s)), nullptr);
        auto lastLoadedPath = lastLoadedPreset ? lastLoadedPreset->getPresetRelativePath() : "";
        stateTree.setProperty("lastLoadedPresetPath", lastLoadedPath, nullptr);

        copyXmlToBinary(*stateTree.createXml(), destData);
    }

    void Processor::setStateInformation(const void *data, int sizeInBytes) {
        auto xml = getXmlFromBinary(data, sizeInBytes);
        if (!xml) return;
        auto stateTree = juce::ValueTree::fromXml(*xml);
        auto stateString = stateTree.getProperty("state").toString();

        try {
            auto s = choc::json::parse(stateString.toStdString());
            queuePreset(Preset::fromState(s, true), false);
        } catch (choc::json::ParseError& e) {
            DBG("unable to load preset");
            DBG(e.what());
        }

        auto lastLoadedPresetPath = resources->getPresetsFolder().getChildFile(
                stateTree.getProperty("lastLoadedPresetPath").toString());

        if (lastLoadedPresetPath.exists()) {
            lastLoadedPreset = {FileBackedPreset::createFromFile(lastLoadedPresetPath)};
        }
    }

    double Processor::getBPM() {
        if (posInfo && posInfo->getBpm().hasValue()) {
            auto bpm = *posInfo->getBpm();
            if (bpm < 0.01) return defaultBPM;
            return bpm;
        }
        return defaultBPM;
    }

    double Processor::getSyncTimeSeconds(float proportionOfBeat) {
        auto timeMult = 1.f;

        if (posInfo) {
            if (posInfo.hasValue()) {
                auto timeSigVal = posInfo->getTimeSignature();
                if (timeSigVal.hasValue()) timeMult = timeSigVal->numerator / (float)timeSigVal->denominator;
            }
        }

        timeMult *= 4;

        return 60.f / getLastBPM() * proportionOfBeat * timeMult;
    }

    double Processor::getLastBPM() const { return lastBPM; }

    double Processor::getNoteLengthSamples(double bpm, float proportionOfBeat, double sampleRate) {
        return proportionOfBeat * getSamplesPerBeat(bpm, sampleRate);
    }

    double Processor::getNoteLengthSamples(float proportionOfBeat) const {
        return getNoteLengthSamples(getLastBPM(), proportionOfBeat, getLastSampleRate());
    }

    float Processor::getSamplesPerBeat() const {
        return getSamplesPerBeat(getLastBPM(), getLastSampleRate());
    }

    float Processor::getSamplesPerBeat(float bpm, double sampleRate) {
        auto secondsPerBeat = 60.f / bpm;
        return secondsPerBeat * sampleRate ;
    }

    float Processor::getNotes(float seconds) {
        auto noteLength = getNoteLengthSamples(1);
        return (seconds * (float)getLastSampleRate()) / (float)noteLength;
    }

    float Processor::getNotesFromSamples(float samples) {
        auto noteLength = getSamplesPerBeat();
        return (samples) / (float)noteLength;
    }

    void Processor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) {
        try {
            // skip smoothing on first buffer
            // in case we changed this parameter before starting the processor
            if (firstBufferFlag) {
                firstBufferFlag = false;
                if (auto mixParam = getParameter("mix")) {
                    mixGain.setCurrentAndTargetValue(mixParam->getProcessorValue());
                }
                if (auto bypassParam = getParameter("bypass")) {
                    bypassGain.setCurrentAndTargetValue(1 - bypassParam->getProcessorValue());
                }

                for (auto param : getPluginParameters()) {
                    param->callValueChangedListeners();
                }
            }

            if (getTotalNumInputChannels() == 0) {
                buffer.clear();
            }

            for (auto param : getPluginParameters()) {
                param->startBlock(buffer.getNumSamples());
            }

            if (nextPreset) {
                loadPreset(*nextPreset);
                nextPreset = nullptr;
            }

            auto oldSampleRate = lastSampleRate.load();
            lastSampleRate = getSampleRate();
            if (!almostEqual(lastSampleRate.load(), oldSampleRate)) {
                bpmListeners.call(&BPMListener::sampleRateChanged, lastSampleRate);
            }

            auto oldBPM = lastBPM.load();
            lastBPM = getBPM();

            if (!almostEqual(lastBPM.load(), oldBPM)) {
                bpmListeners.call(&BPMListener::bpmChanged, lastBPM);
                for (auto& parameter : allParameters) {
                    if (parameter->getConfig()->processorValueChangesWithBPM) {
                        parameter->callValueChangedListeners();
                    }
                }
            }

            playhead = getPlayHead();
            if (playhead) {
                posInfo = playhead->getPosition();

                if (posInfo.hasValue()) {
                    auto playing = posInfo->getIsPlaying();
                    if (playing != lastPlaying) {
                        bpmListeners.call(&BPMListener::playChanged, playing);
                        lastPlaying = playing;
                    }
                }
            }

            for (auto c=0; c < getTotalNumOutputChannels(); c++)
                for(auto s=0; s<buffer.getNumSamples(); s++)
                    dryBufferLatencyCompensationLine.pushSample(c, buffer.getSample(c, s));

            modMatrix.processMatrixUpdates();

            const auto gainStart = bypassGain.getCurrentValue() * mixGain.getCurrentValue();
            const auto gainTarget = bypassGain.getTargetValue() * mixGain.getTargetValue();

            if (gainStart > 0 || gainTarget > 0)
            {
                juce::AudioProcessLoadMeasurer::ScopedTimer s(measurer);
                process(buffer, midiMessages);
            }

            cpuLoad.store(static_cast<float>(measurer.getLoadAsProportion()));

            // apply bypass and mix
            for (auto s=0; s<buffer.getNumSamples(); s++) {
                auto gain = bypassGain.getNextValue();
                gain *= mixGain.getNextValue();
                for (auto c=0; c<getTotalNumOutputChannels(); c++) {
                    auto v = buffer.getSample(c, s) * (gain);
                    v += dryBufferLatencyCompensationLine.popSample(c) * (1 -gain);
                    buffer.setSample(c, s, v);
                }
            }
        } catch (std::exception& e) {
            DBG(e.what());
        }

    }

    void Processor::parameterChanged(imagiro::Parameter *param) {
        if (param->getUID() == "bypass") {
            bypassGain.setTargetValue(1 - param->getBoolValue());
        }
        if (param->getUID() == "mix") {
            mixGain.setTargetValue(param->getProcessorValue());
        }
    }

    void Processor::configChanged(imagiro::Parameter *param) {
        auto changeDetails = AudioProcessorListener::ChangeDetails::getDefaultFlags().withParameterInfoChanged(true);
        updateHostDisplay(changeDetails);
    }

    void Processor::updateTrackProperties(const juce::AudioProcessor::TrackProperties &newProperties) {
        trackProperties = newProperties;
    }

    Preset Processor::createPreset(const juce::String &name, bool isDAWSaveState) {
        Preset p (isDAWSaveState);
        p.setName(name.toStdString());

        for (auto parameter : getPluginParameters()) {
            if (!isDAWSaveState) {
                if (parameter->getUID() == "bypass") continue;
            }
            auto paramState = parameter->getState();
            if (!isDAWSaveState) paramState.locked = false;
            p.addParamState(paramState);
        }


        p.setModMatrix(modMatrix.getSerializedMatrix());

        p.getData().addMember("valueData", getValueData().getState(!isDAWSaveState));

        return p;
    }
    void Processor::loadPreset(FileBackedPreset p) {
        loadPreset(p.getPreset());
    }

    void Processor::loadPreset(Preset preset) {
        for (const auto& paramState : preset.getParamStates()) {
            if (auto param = getParameter(paramState.uid)) {
                if (!param->isLocked() || preset.isDAWSaveState()) {
                    param->setState(paramState);
                }
            }
        }

        modMatrix.loadSerializedMatrix(preset.getModMatrix());

        if (preset.getData().hasObjectMember("valueData")) {
            valueData.loadState(preset.getData()["valueData"], !preset.isDAWSaveState());
        }

        presetListeners.call([&](PresetListener &l) { l.OnPresetChange(preset); });
    }

    void Processor::prepareToPlay(double sampleRate, int samplesPerBlock) {
        lastSampleRate = sampleRate;
        setRateAndBufferSizeDetails(sampleRate, samplesPerBlock);
        modMatrix.prepareToPlay(sampleRate, samplesPerBlock);
        measurer.reset(sampleRate, samplesPerBlock);

        if (getTotalNumOutputChannels() > 0) {
            dryBufferLatencyCompensationLine.prepare({
                sampleRate, static_cast<juce::uint32> (samplesPerBlock), static_cast<juce::uint32> (getTotalNumOutputChannels())
            });
            dryBufferLatencyCompensationLine.setDelay(getLatencySamples());
        }

        for (auto parameter : getPluginParameters()) {
            parameter->prepareToPlay(sampleRate, samplesPerBlock);
        }
    }

    float Processor::getCpuLoad() {
        return cpuLoad.load();
    }

    void Processor::audioProcessorChanged(AudioProcessor *processor, const juce::AudioProcessorListener::ChangeDetails &details) {
        if(details.latencyChanged) {
            const auto newLatencyInSamples = getLatencySamples();
            dryBufferLatencyCompensationLine.setMaximumDelayInSamples(std::max(MAX_DELAY_LINES_SAMPLES_DURATION, newLatencyInSamples));
            dryBufferLatencyCompensationLine.setDelay(newLatencyInSamples);
        }
    }
    void Processor::audioProcessorParameterChanged(AudioProcessor *processor, int parameterIndex, float newValue) {}

    juce::AudioProcessorParameter* Processor::getBypassParameter() const {
        juce::String bypassUID = "bypass";
        if (parameterMap.contains (bypassUID)) {
            return parameterMap.at(bypassUID)->asJUCEParameter();
        }

        return nullptr;
    }
}
