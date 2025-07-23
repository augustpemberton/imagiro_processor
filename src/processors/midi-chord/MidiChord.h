//
// Created by August Pemberton on 08/05/2025.
//

#pragma once
#include "imagiro_processor/src/Processor.h"
#include "Parameters.h"
#include "imagiro_processor/src/AudioTimer.h"
#include <deque>
#include <vector>
#include <cmath>

#include "MidiChordParameterLoader.h"

namespace imagiro {
    class MidiChordProcessor : public Processor {
    public:
        MidiChordProcessor(const int numShifts = 4)
            : Processor(MidiChordProcessorParameters::PARAMETERS_YAML, MidiChordParameterLoader(numShifts)),
              numShifts(numShifts)
        {
            // Cache shift parameters
            for (int i = 0; i < numShifts; ++i) {
                shiftParams.push_back(getParameter("shift-" + std::to_string(i)));
            }
        }

        static BusesProperties getDefaultProperties() {
            return BusesProperties();
        }

        void prepareToPlay(double sampleRate, int samplesPerBlock) override {
            Processor::prepareToPlay(sampleRate, samplesPerBlock);
            currentSampleRate = sampleRate;
            // Clear any pending events
            delayedEvents.clear();
        }

        void process(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) override {
            juce::MidiBuffer outputBuffer;

            // Process new incoming MIDI events
            for (auto event : midiMessages) {
                const auto message = event.getMessage();

                if (message.isNoteOn() || message.isNoteOff()) {
                    // Get parameters
                    float strumTimeSeconds = strum->getProcessorValue();
                    float tensionValue = tension->getProcessorValue(); // -1 to 1

                    // Add original note (no delay)
                    outputBuffer.addEvent(message, event.samplePosition);

                    // Calculate delays for each shifted note
                    for (int i = 0; i < numShifts; ++i) {
                        auto shiftedMessage = juce::MidiMessage(message);
                        int shiftAmount = std::round(shiftParams[i]->getProcessorValue());
                        shiftedMessage.setNoteNumber(shiftedMessage.getNoteNumber() + shiftAmount);

                        // Calculate delay with tension
                        float delayPercentage = calculateDelayPercentage(i, tensionValue);
                        int delaySamples = static_cast<int>(strumTimeSeconds * numShifts * delayPercentage * currentSampleRate);

                        // Schedule delayed note
                        int globalSamplePos = event.samplePosition + delaySamples;
                        delayedEvents.push_back({shiftedMessage, globalSamplePos});
                    }
                } else {
                    // Pass through non-note messages
                    outputBuffer.addEvent(message, event.samplePosition);
                }
            }

            // Process delayed events
            auto it = delayedEvents.begin();
            while (it != delayedEvents.end()) {
                it->delaySamples -= buffer.getNumSamples();

                if (it->delaySamples <= 0) {
                    // Time to output this event
                    int samplePos = std::max(0, buffer.getNumSamples() + it->delaySamples);
                    outputBuffer.addEvent(it->message, samplePos);
                    it = delayedEvents.erase(it);
                } else {
                    ++it;
                }
            }

            // Replace input buffer with output
            midiMessages.swapWith(outputBuffer);
        }

    private:

        float calculateDelayPercentage(const int shiftIndex, const float tension) const {
            const float t = static_cast<float>(shiftIndex+1) / static_cast<float>(numShifts+1);
            const float exponent = fastpow(5, tension);
            const float skewedT = fastpow(t, exponent);
            return skewedT;
        }

        struct DelayedMidiEvent {
            juce::MidiMessage message;
            int delaySamples;
        };

        std::deque<DelayedMidiEvent> delayedEvents;
        double currentSampleRate = 44100.0;
        int numShifts;
        std::vector<Parameter*> shiftParams;

        Parameter* strum {getParameter("strum")};
        Parameter* tension {getParameter("tension")};
    };
}
