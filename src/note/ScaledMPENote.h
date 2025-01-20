//
// Created by August Pemberton on 06/09/2023.
//

#pragma once


#include "./env-shape/SegmentCurve.h"

struct ScaledMPENote : public juce::MPENote {
    using juce::MPENote::MPENote;

    explicit ScaledMPENote(const juce::MPENote& base) : juce::MPENote(base) {
        preSamplerNoteOnVelocity = noteOnVelocity;
        preSamplerNoteOffVelocity = noteOffVelocity;
        initialNoteOnVelocity = noteOnVelocity;
        initialNoteOffVelocity = noteOffVelocity;
    }

    explicit ScaledMPENote(const juce::MPENote& base, const SegmentCurve& userCurve, const SegmentCurve& samplerCurve) : juce::MPENote(base) {
        initialNoteOnVelocity= noteOnVelocity;
        initialNoteOffVelocity = noteOffVelocity;

        preSamplerNoteOnVelocity = juce::MPEValue::fromUnsignedFloat( userCurve.getY(noteOnVelocity.asUnsignedFloat()));
        preSamplerNoteOffVelocity= juce::MPEValue::fromUnsignedFloat( userCurve.getY(noteOffVelocity.asUnsignedFloat()));

        noteOnVelocity = juce::MPEValue::fromUnsignedFloat( samplerCurve.getY(preSamplerNoteOnVelocity.asUnsignedFloat()));
        noteOffVelocity = juce::MPEValue::fromUnsignedFloat( samplerCurve.getY(preSamplerNoteOffVelocity.asUnsignedFloat()));
    }

    juce::MPEValue preSamplerNoteOnVelocity;
    juce::MPEValue preSamplerNoteOffVelocity;
    juce::MPEValue initialNoteOnVelocity;
    juce::MPEValue initialNoteOffVelocity;
};

struct RetunedMPENote : public ScaledMPENote {
    RetunedMPENote() = default;

    RetunedMPENote(ScaledMPENote note, float retuningSemitones = 0)
            : ScaledMPENote(note),
              retuningSemitones(retuningSemitones)
    {

    }

    float getNote() {
        return initialNote + retuningSemitones;
    }
    float getNoteWithPitchbend() {
        return initialNote + retuningSemitones + totalPitchbendInSemitones;
    }

    void setRetuning(float retuningSemitones) {
        this->retuningSemitones = retuningSemitones;
    }

    float retuningSemitones {0};
};
