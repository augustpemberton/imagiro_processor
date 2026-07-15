#pragma once

#include "NoteInfo.h"
#include <imagiro_util/structures/beman/inplace_vector.h>
#include <cstdint>
#include <functional>

namespace imagiro {

// Tracks held notes and sustain (CC64) so the raw-MIDI and the structured
// (CLAP) note dialects share one release/sustain path. Notes are keyed by
// (channel, note). A note-on for an already-held note releases the old voice
// before starting the new one; raw-MIDI note-on velocity 0 is a note-off.
// Sustain is honored on the zone-master channel 1 only but holds notes on all
// channels (full-lower-zone convention). Choke removes a note immediately,
// bypassing sustain, and is reported distinctly from a note-off.
class NoteTracker {
public:
    std::function<void(const NoteInfo&)> onNoteOn;
    std::function<void(const NoteInfo&)> onNoteOff;
    std::function<void(const NoteInfo&)> onNoteChoke;

    template <class Synth>
    void connect(Synth& synth) {
        onNoteOn  = [&synth](const NoteInfo& note) { synth.noteOn(note); };
        onNoteOff = [&synth](const NoteInfo& note) { synth.noteOff(note); };
    }

    // ---- structured note dialect (CLAP notes) ----
    void noteOn(const NoteInfo& note)   { keyDown(note); }
    void noteOff(const NoteInfo& note)  { keyUp(note); }
    void noteChoke(const NoteInfo& note){ choke(note); }

    // ---- raw MIDI dialect ----
    void processMessage(uint8_t status, uint8_t data1, uint8_t data2) {
        int channel = (status & 0x0F) + 1;
        switch (status & 0xF0) {
            case 0x90:
                if (data2 == 0) keyUp(makeNoteInfo(channel, data1, 64));
                else keyDown(makeNoteInfo(channel, data1, data2));
                break;
            case 0x80:
                keyUp(makeNoteInfo(channel, data1, data2));
                break;
            case 0xB0:
                if (data1 == 64 && channel == kMasterChannel)
                    setSustain(data2 >= 64);
                break;
            default:
                break;
        }
    }

    void processMessage(const uint8_t* msg) {
        processMessage(msg[0], msg[1], msg[2]);
    }

    bool isSustainDown() const { return sustainDown_; }
    int numHeldNotes() const { return static_cast<int>(notes_.size()); }

private:
    static constexpr int kMasterChannel = 1;

    enum class KeyState { down, downAndSustained, sustained };

    struct HeldNote {
        NoteInfo info;
        KeyState state;
    };

    static NoteInfo makeNoteInfo(int channel, int note, int velocity7Bit) {
        return {
            (channel << 7) + note,
            channel,
            note,
            velocity01From7Bit(velocity7Bit)
        };
    }

    // 7-bit MIDI value expanded to 14-bit, then normalized to [0,1].
    static float velocity01From7Bit(int v) {
        int value14Bit = v <= 64
            ? v << 7
            : static_cast<int>(8191.f * (static_cast<float>(v - 64) / 63.f)) + 8192;
        return static_cast<float>(value14Bit) / 16383.f;
    }

    HeldNote* find(int channel, int note) {
        for (auto& n : notes_)
            if (n.info.midiChannel == channel && n.info.initialNote == note) return &n;
        return nullptr;
    }

    void remove(const HeldNote* note) {
        notes_.erase(notes_.begin() + (note - notes_.data()));
    }

    void emitOn(const NoteInfo& info)  { if (onNoteOn) onNoteOn(info); }
    void emitOff(const NoteInfo& info) { if (onNoteOff) onNoteOff(info); }
    void emitChoke(const NoteInfo& info) {
        if (onNoteChoke) onNoteChoke(info);
        else if (onNoteOff) onNoteOff(info);
    }

    void keyDown(const NoteInfo& note) {
        if (auto* existing = find(note.midiChannel, note.initialNote)) {
            emitOff(existing->info);
            remove(existing);
        }
        if (notes_.size() == notes_.capacity()) return;
        notes_.push_back({note,
                          sustainDown_ ? KeyState::downAndSustained : KeyState::down});
        emitOn(note);
    }

    void keyUp(const NoteInfo& note) {
        auto* held = find(note.midiChannel, note.initialNote);
        if (!held) return;
        if (held->state == KeyState::downAndSustained) {
            held->state = KeyState::sustained;
        } else {
            emitOff(held->info);
            remove(held);
        }
    }

    void choke(const NoteInfo& note) {
        if (auto* held = find(note.midiChannel, note.initialNote)) {
            emitChoke(held->info);
            remove(held);
        } else {
            emitChoke(note);
        }
    }

    void setSustain(bool isDown) {
        sustainDown_ = isDown;
        for (int i = static_cast<int>(notes_.size()) - 1; i >= 0; i--) {
            auto& n = notes_[static_cast<size_t>(i)];
            if (n.state == KeyState::down && isDown) {
                n.state = KeyState::downAndSustained;
            } else if (n.state == KeyState::sustained && !isDown) {
                emitOff(n.info);
                notes_.erase(notes_.begin() + i);
            } else if (n.state == KeyState::downAndSustained && !isDown) {
                n.state = KeyState::down;
            }
        }
    }

    beman::inplace_vector<HeldNote, 128> notes_;
    bool sustainDown_ = false;
};

} // namespace imagiro
