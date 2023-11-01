//
// Created by August Pemberton on 12/09/2022.
//

#include "Processor.h"
#include "choc/text/choc_JSON.h"

namespace imagiro {

    Processor::Processor(juce::String currentVersion, juce::String productSlug)
            : versionManager(currentVersion, productSlug)
    {
        bypassGain.reset(250);
    }

    Processor::Processor(const juce::AudioProcessor::BusesProperties &ioLayouts,
                         juce::String currentVersion, juce::String productSlug)
            : ProcessorBase(ioLayouts),
            versionManager(currentVersion, productSlug)
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

    void Processor::randomizeParameters() {
        auto rng = juce::Random::getSystemRandom();
        for (auto param : getPluginParameters()) {
            param->setValueNotifyingHost(rng.nextFloat());
        }
    }

    void Processor::queuePreset(FileBackedPreset p, bool waitUntilAudioThread) {
        lastLoadedPreset = p;
        queuePreset(p.getPreset(), waitUntilAudioThread);
    }

    void Processor::queuePreset(Preset p, bool waitUntilAudioThread) {
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
        } catch (choc::json::ParseError e) {
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

    double Processor::getNoteLengthSamples(float proportionOfBeat) {
        return getNoteLengthSamples(getLastBPM(), proportionOfBeat, getLastSampleRate());
    }

    float Processor::getSamplesPerBeat() {
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
        for (auto param : getPluginParameters()) {
            param->generateSmoothedValueBlock(buffer.getNumSamples());
        }

        if (nextPreset) {
            loadPreset(*nextPreset);
            nextPreset = nullptr;
        }

        auto oldSampleRate = lastSampleRate;
        lastSampleRate = getSampleRate();
        if (lastSampleRate != oldSampleRate) {
            bpmListeners.call(&BPMListener::sampleRateChanged, lastSampleRate);
        }

        auto oldBPM = lastBPM.load();
        lastBPM = getBPM();

        if (lastBPM != oldBPM) {
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

        process(buffer, midiMessages);

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

    Preset Processor::createPreset(const juce::String &name, bool isDawPreset) {
        Preset p;
        p.setName(name.toStdString());

        for (auto parameter : getPluginParameters()) {
            p.addParamState(parameter->getState());
        }

        p.getData().addMember("scales", getScalesState());

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

        loadScalesTree(preset.getData()["scales"]);

        presetListeners.call([&](PresetListener &l) { l.OnPresetChange(preset); });
    }

    void Processor::setScale(const juce::String& scaleID, juce::BigInteger state) {
        scales[scaleID].setState(state);
    }

    Scale *Processor::getScale(juce::String scaleID) {
        if (scales.count(scaleID)) return &scales[scaleID];
        return nullptr;
    }

    choc::value::Value Processor::getScalesState() {
        auto presetScales = choc::value::createEmptyArray();
        for (auto& scale : scales) {
            auto scaleTree = choc::value::createObject("Scale");
            scaleTree.addMember("notes", scale.second.getState().toString(2).toStdString());
            scaleTree.addMember("id", scale.first.toStdString());
            presetScales.addArrayElement(scaleTree);
        }
        return presetScales;
    }

    void Processor::loadScalesTree(const choc::value::ValueView& t) {
        // reset all current scales in case preset doesn't contain any
        for (const auto& scale : scales) {
            scales[scale.first].setState(Scale({0}).getState());
        }

        if (!t.isArray()) return;

        for (auto scaleTree : t) {
            juce::BigInteger scaleState;
            if (!scaleTree.hasObjectMember("notes")) continue;
            if (!scaleTree.hasObjectMember("id")) continue;

            scaleState.parseString(scaleTree["notes"].toString(), 2);
            setScale(scaleTree["id"].toString(), scaleState);
        }
    }

    void Processor::prepareToPlay(double sampleRate, int samplesPerBlock) {
        dryBuffer.setSize(getTotalNumInputChannels(), samplesPerBlock);
        for (auto parameter : getPluginParameters()) {
            parameter->prepareToPlay(sampleRate, samplesPerBlock);
        }
    }
}
