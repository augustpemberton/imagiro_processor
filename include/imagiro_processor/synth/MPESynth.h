//
// Created by August Pemberton on 28/05/2025.
//

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "../note/ScaledMPENote.h"
#include <imagiro_util/structures/FixedHashSet.h>
#include <imagiro_util/structures/beman/inplace_vector.h>

class MPESynth : public juce::MPESynthesiserBase {
public:
    struct Listener {
        virtual ~Listener() = default;

        virtual void onVoiceStarted(size_t voiceIndex) {}
        virtual void onVoiceReleased(size_t voiceIndex) {}
        virtual void onVoiceFinished(size_t voiceIndex) {}
        virtual void onSynthSubBlockStarted(MPESynth& synth, int startSample, int numSamples) {}
    };

    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

    explicit MPESynth(const int maxVoices = MAX_MOD_VOICES / 2) {
        setMaxVoices(maxVoices);
    }

    void renderNextSubBlock(juce::AudioSampleBuffer& out, int startSample, int numSamples) override {
        listeners.call(&Listener::onSynthSubBlockStarted, *this, startSample, numSamples);
        processVoiceCommands();
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
        startVoiceCommands.push_back({note, *freeVoiceIndex});
        listeners.call(&Listener::onVoiceStarted, *freeVoiceIndex);
    }

    void noteReleased(juce::MPENote finishedNote) override {
        const auto note = RetunedMPENote(ScaledMPENote(finishedNote));

        for (auto i=0; i<MAX_MOD_VOICES; i++) {
            const auto& n = playingNotes[i];
            if (!n) continue;

            if (n->noteID == note.noteID) {
                stopVoiceCommands.push_back({static_cast<size_t>(i), false});
                listeners.call(&Listener::onVoiceReleased, i);
            }
        }
    }

    void processVoiceCommands() {
        for (const auto& [note, voiceIndex] : startVoiceCommands) {
            startVoice(note, voiceIndex);
        }
        for (const auto& [voiceIndex, quickstop] : stopVoiceCommands) {
            stopVoice(voiceIndex, quickstop);
        }

        startVoiceCommands.clear();
        stopVoiceCommands.clear();
    }

    void handleMidiEvent(const juce::MidiMessage &m) override {
        MPESynthesiserBase::handleMidiEvent(m);
    }

    void notePressureChanged(const juce::MPENote n) override {}
    void notePitchbendChanged(const juce::MPENote n) override {}

    virtual void startVoice(RetunedMPENote note, size_t voiceIndex) = 0;
    virtual void stopVoice(size_t voiceIndex, bool quickStop) = 0;

    const auto& getActiveVoices() const { return activeVoices; }

protected:
    int maxVoices;

    juce::ListenerList<Listener> listeners;

    struct StartVoiceCommand {
        RetunedMPENote note;
        size_t voiceIndex;
    };
    struct StopVoiceCommand {
        size_t voiceIndex;
        bool quickstop;
    };

    beman::inplace_vector<StartVoiceCommand, 128> startVoiceCommands;
    beman::inplace_vector<StopVoiceCommand, 128> stopVoiceCommands;

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

    virtual bool isVoiceReadyToStart(size_t voiceIndex) {
        return true;
    }

    std::optional<size_t> findFreeVoice() {
        std::optional<size_t> voiceIndex = {};
        for (auto i = 0u; i < playingNotes.size(); i++) {
            if (!playingNotes[i].has_value() && isVoiceReadyToStart(i)) {
                voiceIndex = i;
                break;
            }
        }

        if (!voiceIndex.has_value()) return {};

        if (activeVoices.size() >= maxVoices) {
            auto oldestVoiceIt = voiceAgeQueue.begin();
            auto oldestVoice = *oldestVoiceIt;
            stopVoice(oldestVoice, true);
            voiceAgeQueue.erase(oldestVoiceIt);
        }

        return voiceIndex;
    }

    void setNoteForVoice(const size_t voiceIndex, RetunedMPENote note) {
        activeVoices.insert(voiceIndex);
        playingNotes[voiceIndex] = note;
    }

    void clearNoteForVoice(const size_t voiceIndex) {
        playingNotes[voiceIndex].reset();
        activeVoices.erase(voiceIndex);
    }
};
