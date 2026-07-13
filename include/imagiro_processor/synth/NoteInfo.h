#pragma once

namespace imagiro {

// Framework-free note descriptor passed from a plugin shell into a core synth.
struct NoteInfo {
    int noteId = 0;
    int midiChannel = 0;
    int initialNote = 0;
    float velocity01 = 0.f;
};

} // namespace imagiro
