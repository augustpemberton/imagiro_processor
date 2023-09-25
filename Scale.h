//
// Created by August Pemberton on 07/03/2023.
//

#pragma once
#include <juce_core/juce_core.h>

#include <utility>

namespace imagiro {
    class Scale {
    public:
        struct Listener {
            virtual void scaleUpdated() {}
        };

        Scale(const std::vector<int>& enabledNotes = {0,1,2,3,4,5,6,7,8,9,10,11}) {
            //pitchVector.reserve(12);
            for (auto i : enabledNotes) {
                setSemitone(i, true);
            }
        }

        double getQuantized(double semitones) {
            semitones = round(semitones);
            auto scale = floor(semitones / 12.f) * 12.f;
            auto offset = semitones - scale;

            auto scaleBelow = getSnapped(offset) + scale;
            auto scaleAbove = getSnapped(offset) + scale + 12;

            auto distBelow = abs(scaleBelow - semitones);
            auto distAbove = abs(scaleAbove - semitones);

            return distAbove < distBelow ? scaleAbove : scaleBelow;
        }

        juce::BigInteger getState() {
            return enabledSemitones;
        }

        void setState(juce::BigInteger state) {
            enabledSemitones = std::move(state);
            listeners.call(&Listener::scaleUpdated);
        }

        void setSemitone(int semitone, bool enabled) {
            enabledSemitones.setBit(semitone, enabled);
            listeners.call(&Listener::scaleUpdated);
        }

        bool operator==(const Scale& rhs) const {
            return enabledSemitones == rhs.enabledSemitones;
        }

        void addListener(Listener* l) { listeners.add(l); }
        void removeListener(Listener* l) { listeners.remove(l); }

    private:
        juce::BigInteger enabledSemitones;

        juce::ListenerList<Listener> listeners;

        double getSnapped(double p) {
            std::vector<double> pitchVector;

            for (auto i = 0; i < 12; i++) {
                if (enabledSemitones[i]) pitchVector.push_back(i);
            }

            return getSnappedPitch(p, pitchVector);
        }
    };
}
