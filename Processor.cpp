//
// Created by August Pemberton on 12/09/2022.
//

#include "Processor.h"
//#include <gin_dsp/gin_dsp.h>
//#include <imagiro_gui/imagiro_gui.h>

namespace imagiro {

    Processor::Processor()
    {
        lastLoadedPreset = std::make_unique<Preset>();
    }

    Processor::Processor(const juce::AudioProcessor::BusesProperties &ioLayouts)
            : ProcessorBase(ioLayouts)
    {
        lastLoadedPreset = std::make_unique<Preset>();
    }

    void Processor::reset() {
        for (auto p : getPluginParameters())
            p->reset();
    }

    void Processor::addPluginParameter (Parameter* p) {
        addParameter (p);
        allParameters.add (p);
        parameterMap[p->getUID()] = p;
    }

    Parameter* Processor::addParam (std::unique_ptr<Parameter> p) {
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

    void Processor::queuePreset(Preset preset, bool waitUntilAudioThread) {
        if (waitUntilAudioThread) nextPreset = std::make_unique<Preset>(preset);
        else loadPreset(preset);
    }

    void Processor::getStateInformation(juce::MemoryBlock &destData) {
        copyXmlToBinary(*createPreset(
                lastLoadedPreset ? lastLoadedPreset->getName() : "init", true)
                .getTree().createXml(), destData);
    }

    void Processor::setStateInformation(const void *data, int sizeInBytes) {
        std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
        auto presetTree = juce::ValueTree::fromXml(*xmlState);
        loadPreset(Preset(presetTree));
    }

    double Processor::getBPM() {
        if (posInfo && posInfo->getBpm().hasValue()) {
            auto bpm = *posInfo->getBpm();
            if (bpm == 0) bpm = 120;
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

        if (nextPreset) {
            loadPreset(*nextPreset);
            nextPreset = nullptr;
        }

        TRACE_DSP();
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
    }

    void Processor::updateTrackProperties(const juce::AudioProcessor::TrackProperties &newProperties) {
        trackProperties = newProperties;
    }

    Preset Processor::createPreset(const juce::String &name, bool isDawPreset) {
        Preset p;
        p.setName(name);

        for (auto parameter : getPluginParameters()) {
            p.addParamState(parameter->getState());
        }

        p.getTree().appendChild(modMatrix.getTree(), nullptr);
        p.getTree().appendChild(getScalesTree(), nullptr);

        return p;
    }

    void Processor::loadPreset(Preset preset) {
        for (const auto& paramState : preset.getParamStates()) {
            if (auto param = getParameter(paramState.uid)) {
                param->setUserValueAsUserAction(paramState.value);
            }
        }

        loadScalesTree(preset.getTree().getChildWithName("scales"));

        modMatrix.loadFromTree(preset.getTree().getOrCreateChildWithName("modmatrix", nullptr));

        lastLoadedPreset = std::make_unique<Preset>(preset);
        presetListeners.call([&](PresetListener &l) { l.OnPresetChange(preset); });
    }

    void Processor::setScale(const juce::String& scaleID, juce::BigInteger state) {
        scales[scaleID].setState(state);
    }

    Scale *Processor::getScale(juce::String scaleID) {
        if (scales.count(scaleID)) return &scales[scaleID];
        return nullptr;
    }

    juce::ValueTree Processor::getScalesTree() {
        juce::ValueTree presetScales ("scales");
        for (auto& scale : scales) {
            juce::ValueTree scaleTree("scale");
            scaleTree.setProperty("notes", scale.second.getState().toString(2),
                                  nullptr);
            scaleTree.setProperty("id", scale.first, nullptr);
            presetScales.appendChild(scaleTree, nullptr);
        }
        return presetScales;
    }

    void Processor::loadScalesTree(juce::ValueTree t) {
        // reset all current scales in case preset doesn't contain any
        for (const auto& scale : scales) {
            scales[scale.first].setState(Scale({0}).getState());
        }

        if (!t.isValid()) return;

        for (auto scaleTree : t) {
            juce::BigInteger state;
            if (!scaleTree.hasProperty("notes")) continue;
            if (!scaleTree.hasProperty("id")) continue;

            state.parseString(scaleTree.getProperty("notes").toString(), 2);
            setScale(scaleTree.getProperty("id").toString(), state);
        }
    }
}
