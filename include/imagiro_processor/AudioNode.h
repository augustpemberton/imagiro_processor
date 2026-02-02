//
// Created by August Pemberton on 31/12/2025.
//

#pragma once
#include "juce_audio_basics/juce_audio_basics.h"
#include "parameter/ParamController.h"
#include "parameter/ParamCreator.h"
#include "processor/state/ProcessState.h"

namespace imagiro {
    class AudioNode {
    public:
        virtual ~AudioNode() = default;

        void prepare(double sampleRate, int blockSize, int numChannels) {
            sampleRate_ = sampleRate;
            blockSize_ = blockSize;
            numChannels_ = numChannels;
            onPrepare();
        }

        virtual void createParams(const ParamCreator& /*creator*/) { }
        virtual void process(juce::AudioBuffer<float>& buffer,
                            juce::MidiBuffer& midi,
                            const ProcessState& state) = 0;
        virtual void setRealtime(bool /*realtime*/) {}

    protected:
        virtual void onPrepare() {}

        double sampleRate_{44100.0};
        int blockSize_{512};
        int numChannels_{2};
    };
}