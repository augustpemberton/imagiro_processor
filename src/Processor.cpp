//
// Created by August Pemberton on 12/09/2022.
//

#include "Processor.h"
#include "choc/text/choc_JSON.h"

namespace imagiro {

    Processor::Processor(const juce::String& currentVersion, const juce::String& productSlug)
            : versionManager(currentVersion, productSlug),
              paramLoader(*this, getParametersYAMLString())
    {
        bypassGain.reset(250);
    }

    Processor::Processor(const juce::AudioProcessor::BusesProperties &ioLayouts,
                         const juce::String& currentVersion, const juce::String& productSlug)
            : ProcessorBase(ioLayouts),
              versionManager(currentVersion, productSlug),
              paramLoader(*this, getParametersYAMLString())
    {
        bypassGain.reset(250);
    }

    Processor::~Processor() {
        for (auto p : getPluginParameters())
            if (p->getUID() == "bypass") p->removeListener(this);
    }

    void Processor::reset() {
        for (auto p : getPluginParameters())
            p->reset();
    }

    Parameter* Processor::addParam (std::unique_ptr<Parameter> p) {
        if (p->getUID() == "bypass") p->addListener(this);

        auto rawPtr = p.get();
        allParameters.add (rawPtr);
        parameterMap[p->getUID()] = rawPtr;
        if (p->isInternal()) internalParameters.add (p.release());
        else addParameter (p.release());
        return rawPtr;
    }

    Parameter* Processor::getParameter (const juce::String& uid)
    {
        if (parameterMap.find (uid) != parameterMap.end())
            return parameterMap[uid];

        return nullptr;
    }

    const juce::Array<Parameter*>& Processor::getPluginParameters() { return allParameters; }


    //================================================
    void Processor::addPresetListener(PresetListener *l) { presetListeners.add(l); }
    void Processor::removePresetListener(PresetListener *l) { presetListeners.remove(l); }

    void Processor::queuePreset(const FileBackedPreset& p, bool waitUntilAudioThread) {
        lastLoadedPreset = p;
        queuePreset(p.getPreset(), waitUntilAudioThread);
    }

    void Processor::queuePreset(const Preset& p, bool waitUntilAudioThread) {
        if (waitUntilAudioThread) nextPreset = std::make_unique<Preset>(p);
        else loadPreset(p);
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
            loadPreset(Preset::fromState(s));
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
            if (bpm < 0.01) bpm = 120;
            return bpm;
        }
        return 120;
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
#if JucePlugin_IsSynth
        buffer.clear();
#endif
        if (!authManager.isAuthorized()) return;

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
        }

        playhead = getPlayHead();
        posInfo = playhead->getPosition();

        if (posInfo.hasValue()) {
            auto playing = posInfo->getIsPlaying();
            if (playing != lastPlaying) {
                bpmListeners.call(&BPMListener::playChanged, playing);
                lastPlaying = playing;
            }
        }

        for (auto c=0; c<dryBuffer.getNumChannels(); c++) {
            dryBuffer.copyFrom(c, 0,
                               buffer.getReadPointer(c),
                               buffer.getNumSamples());
        }

        {
            juce::AudioProcessLoadMeasurer::ScopedTimer s(measurer);
            process(buffer, midiMessages);
        }

        cpuLoad.store(static_cast<float>(measurer.getLoadAsProportion()));

        // apply bypass
        for (auto s=0; s<buffer.getNumSamples(); s++) {
            auto gain = bypassGain.getNextValue();
            for (auto c=0; c<dryBuffer.getNumChannels(); c++) {
                auto v = buffer.getSample(c, s) * (1-gain);
                v += dryBuffer.getSample(c, s) * gain;
                buffer.setSample(c, s, v);
            }
        }
    }

    void Processor::parameterChanged(imagiro::Parameter *param) {
        if (param->getUID() == "bypass") {
            bypassGain.setTargetValue(param->getValue());
        }
    }

    void Processor::updateTrackProperties(const juce::AudioProcessor::TrackProperties &newProperties) {
        trackProperties = newProperties;
    }

    Preset Processor::createPreset(const juce::String &name, bool isDAWSaveState) {
        Preset p;
        p.setName(name.toStdString());

        for (auto parameter : getPluginParameters()) {
            if (parameter->getUID() == "bypass") continue;
            p.addParamState(parameter->getState());
        }

        return p;
    }
    void Processor::loadPreset(FileBackedPreset p) {
        loadPreset(p.getPreset());
    }

    void Processor::loadPreset(Preset preset) {
        for (const auto& paramState : preset.getParamStates()) {
            if (auto param = getParameter(paramState.uid)) {
                param->setUserValueAsUserAction(paramState.value);
            }
        }

        presetListeners.call([&](PresetListener &l) { l.OnPresetChange(preset); });
    }

    void Processor::prepareToPlay(double sampleRate, int samplesPerBlock) {
        measurer.reset(sampleRate, samplesPerBlock);
        dryBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock);
        for (auto parameter : getPluginParameters()) {
            parameter->prepareToPlay(sampleRate, samplesPerBlock);
        }
    }

    juce::String Processor::getParametersYAMLString() {
#if JUCE_DEBUG && defined(SRCPATH)
        auto file = juce::File(juce::String(SRCPATH) + "/Parameters.yaml");
        return file.loadFileAsString();
#else
        return BinaryData::Parameters_yaml;
#endif
    }

    float Processor::getCpuLoad() {
        return cpuLoad.load();
    }
}
