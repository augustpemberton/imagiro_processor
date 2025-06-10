//
// Created by August Pemberton on 28/05/2025.
//

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

#include "../parameter/modulation/ModMatrix.h"
#include "../parameter/modulation/MultichannelValue.h"

class MPESynth : public juce::MPESynthesiserBase {
public:
    struct Listener {
        virtual ~Listener() = default;

        virtual void onVoiceStarted(size_t voiceIndex) {}
        virtual void onVoiceReleased(size_t voiceIndex) {}
        virtual void onVoiceFinished(size_t voiceIndex) {}
    };

    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

    void handleMidiEvent(const juce::MidiMessage &m) override {
        if (m.isController() && m.getControllerNumber() == 1) {
            const float modValue = static_cast<float>(m.getControllerValue()) / 127.f;
            modWheelValue.setGlobalValue(modValue);
        }

        MPESynthesiserBase::handleMidiEvent(m);
        activeVoices.reserve(MAX_MOD_VOICES);
    }

    void notePressureChanged(const juce::MPENote n) override {
        for (auto i = 0u; i < MAX_MOD_VOICES; i++) {
            pressureValue.setVoiceValue(n.pressure.asUnsignedFloat(), i);
        }
    }

    void notePitchbendChanged(const juce::MPENote n) override {
        for (auto i = 0u; i < MAX_MOD_VOICES; i++) {
            const auto pitchBendRange = getLegacyModePitchbendRange();
            const auto pitchbendProportion = n.totalPitchbendInSemitones / pitchBendRange;
            pitchbendValue.setVoiceValue(static_cast<float>(pitchbendProportion), i);
        }
    }

    const auto &getPressure() { return pressureValue; }
    const auto &getPitchbend() { return pitchbendValue; }
    const auto &getInitialNote() { return initialNoteValue; }
    const auto &getInitialVelocity() { return initialVelocityValue; }
    const auto &getModWheel() { return modWheelValue; }

    const auto& getActiveVoices() { return activeVoices; }

protected:
    juce::ListenerList<Listener> listeners;

    MultichannelValue<MAX_MOD_VOICES> pressureValue {false};
    MultichannelValue<MAX_MOD_VOICES> pitchbendValue {true};
    MultichannelValue<MAX_MOD_VOICES> initialNoteValue {true};
    MultichannelValue<MAX_MOD_VOICES> initialVelocityValue {false};
    MultichannelValue<MAX_MOD_VOICES> modWheelValue {false};
    std::unordered_set<size_t> activeVoices;

    void voiceStarted(size_t voiceIndex) {
        listeners.call(&Listener::onVoiceStarted, voiceIndex);
    }

    void voiceReleased(size_t voiceIndex) {
        listeners.call(&Listener::onVoiceReleased, voiceIndex);
    }

    void voiceFinished(size_t voiceIndex) {
        listeners.call(&Listener::onVoiceFinished, voiceIndex);
    }

    void setNoteForVoice(const size_t voiceIndex, RetunedMPENote note,
                         const juce::NormalisableRange<float> &pitchRange = {-36, 36}) {

        const auto initialNoteOffset = note.getNote() - 60;
        const auto noteProportion = (initialNoteOffset - pitchRange.getRange().getStart())
                                    / pitchRange.getRange().getLength();
        initialNoteValue.setVoiceValue((noteProportion - 0.5f) * 2, voiceIndex);

        initialVelocityValue.setVoiceValue(note.noteOnVelocity.asUnsignedFloat(), voiceIndex);
        pressureValue.setVoiceValue(note.pressure.asUnsignedFloat(), voiceIndex);

        const auto pitchBendRange = getZoneLayout().getLowerZone().perNotePitchbendRange;
        const auto pitchbendProportion = note.totalPitchbendInSemitones / pitchBendRange;
        pitchbendValue.setVoiceValue(static_cast<float>(pitchbendProportion), voiceIndex);

        activeVoices.insert(voiceIndex);
    }

    void clearNoteForVoice(const size_t voiceIndex) {
        initialNoteValue.setVoiceValue(0, voiceIndex);
        initialVelocityValue.setVoiceValue(0, voiceIndex);
        pressureValue.setVoiceValue(0, voiceIndex);
        pitchbendValue.setVoiceValue(0, voiceIndex);
        activeVoices.erase(voiceIndex);
    }
};
