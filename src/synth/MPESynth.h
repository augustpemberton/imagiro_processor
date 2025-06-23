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

    explicit MPESynth(const int maxVoices = MAX_MOD_VOICES / 2){
        setMaxVoices(maxVoices);
    }

    void setMaxVoices(int maxVoices) {
        // need half as many max voices as mod voices, so we can do stealing without clicks
        maxVoices = std::min(MAX_MOD_VOICES / 2, maxVoices);
        this->maxVoices = maxVoices;

        for (auto& v : activeVoices) {
            stopVoice(v, true);
        }
    }

    void noteAdded(juce::MPENote newNote) override {
        const auto freeVoiceIndex = findFreeVoice();
        if (!freeVoiceIndex) return;

        const auto note = RetunedMPENote(ScaledMPENote(newNote));

        voiceAgeQueue.push_back(*freeVoiceIndex);
        setNoteForVoice(*freeVoiceIndex, note);
        startVoice(note, *freeVoiceIndex);
        listeners.call(&Listener::onVoiceStarted, *freeVoiceIndex);
    }

    void noteReleased(juce::MPENote finishedNote) override {
        const auto note = RetunedMPENote(ScaledMPENote(finishedNote));

        for (auto i=0; i<MAX_MOD_VOICES; i++) {
            const auto& n = playingNotes[i];
            if (!n) continue;

            if (n->noteID == note.noteID) {
                stopVoice(i, false);
                listeners.call(&Listener::onVoiceReleased, i);
            }
        }
    }

    void handleMidiEvent(const juce::MidiMessage &m) override {
        if (m.isController() && m.getControllerNumber() == 1) {
            const float modValue = static_cast<float>(m.getControllerValue()) / 127.f;
            modWheelValue.setGlobalValue(modValue);
        }

        MPESynthesiserBase::handleMidiEvent(m);
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

    virtual void startVoice(RetunedMPENote note, size_t voiceIndex) = 0;
    virtual void stopVoice(size_t voiceIndex, bool quickStop) = 0;

    const auto &getPressure() { return pressureValue; }
    const auto &getPitchbend() { return pitchbendValue; }
    const auto &getInitialNote() { return initialNoteValue; }
    const auto &getInitialVelocity() { return initialVelocityValue; }
    const auto &getModWheel() { return modWheelValue; }

    const auto& getActiveVoices() const { return activeVoices; }

protected:
    int maxVoices;

    juce::ListenerList<Listener> listeners;

    MultichannelValue<MAX_MOD_VOICES> pressureValue {false};
    MultichannelValue<MAX_MOD_VOICES> pitchbendValue {true};
    MultichannelValue<MAX_MOD_VOICES> initialNoteValue {true};
    MultichannelValue<MAX_MOD_VOICES> initialVelocityValue {false};
    MultichannelValue<MAX_MOD_VOICES> modWheelValue {false};

    FixedHashSet<size_t, MAX_MOD_VOICES> activeVoices = {};
    std::array<std::optional<RetunedMPENote>, MAX_MOD_VOICES> playingNotes;
    std::deque<size_t> voiceAgeQueue;

    void voiceFinished(size_t voiceIndex) {
        playingNotes[voiceIndex].reset();

        const auto ageIt = std::ranges::find(voiceAgeQueue, voiceIndex);
        if (ageIt != voiceAgeQueue.end()) {
            voiceAgeQueue.erase(ageIt);
        }

        clearNoteForVoice(voiceIndex);
        listeners.call(&Listener::onVoiceFinished, voiceIndex);
    }

    std::optional<size_t> findFreeVoice() {
        if (activeVoices.size() >= maxVoices) {
            auto oldestVoiceIt = voiceAgeQueue.begin();
            auto oldestVoice = *oldestVoiceIt;
            stopVoice(oldestVoice, true);
            voiceAgeQueue.erase(oldestVoiceIt);
        }

        for (auto i = 0u; i < playingNotes.size(); i++) {
            if (!playingNotes[i].has_value()) return i;
        }

        // This shouldn't happen if our tracking is correct
        jassertfalse;
        return {};
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
        playingNotes[voiceIndex] = note;
    }

    void clearNoteForVoice(const size_t voiceIndex) {
        playingNotes[voiceIndex].reset();
        activeVoices.erase(voiceIndex);

        initialNoteValue.setVoiceValue(0, voiceIndex);
        initialVelocityValue.setVoiceValue(0, voiceIndex);
        pressureValue.setVoiceValue(0, voiceIndex);
        pitchbendValue.setVoiceValue(0, voiceIndex);
    }
};
