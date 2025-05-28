//
// Created by August Pemberton on 28/05/2025.
//

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

#include "imagiro_processor/src/parameter/modulation/ModMatrix.h"
#include "imagiro_processor/src/parameter/modulation/MultichannelValue.h"


class MPESynth : public juce::MPESynthesiserBase {
public:
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
            const auto pitchBendRange = getZoneLayout().getLowerZone().perNotePitchbendRange;
            const auto pitchbendProportion = n.totalPitchbendInSemitones / pitchBendRange;
            pitchbendValue.setVoiceValue(static_cast<float>(pitchbendProportion), i);
        }
    }

    const auto &getPressure() { return pressureValue; }
    const auto &getPitchbend() { return pitchbendValue; }
    const auto &getInitialNote() { return initialNoteValue; }
    const auto &getInitialVelocity() { return initialVelocityValue; }
    const auto &getModWheel() { return modWheelValue; }

protected:
    MultichannelValue<MAX_MOD_VOICES> pressureValue;
    MultichannelValue<MAX_MOD_VOICES> pitchbendValue;
    MultichannelValue<MAX_MOD_VOICES> initialNoteValue;
    MultichannelValue<MAX_MOD_VOICES> initialVelocityValue;
    MultichannelValue<MAX_MOD_VOICES> modWheelValue;

    void setNoteForVoice(size_t voiceIndex, RetunedMPENote note,
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
    }

    void clearNoteForVoice(const size_t voiceIndex) {
        initialNoteValue.setVoiceValue(0, voiceIndex);
        initialVelocityValue.setVoiceValue(0, voiceIndex);
        pressureValue.setVoiceValue(0, voiceIndex);
        pitchbendValue.setVoiceValue(0, voiceIndex);
    }
};
