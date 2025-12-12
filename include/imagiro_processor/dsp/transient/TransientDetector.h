//
// Created by August Pemberton on 09/09/2023.
//

#pragma once
#include "Transient.h"
#include <imagiro_processor/dsp/EnvelopeFollower.h>
#include "juce_audio_basics/juce_audio_basics.h"

namespace imagiro {
    class TransientDetector {
    public:
        TransientDetector(float sensitivity, float lookaheadSeconds = 0.2f);

        std::vector<Transient> getTransients(const juce::AudioSampleBuffer& buffer, float dbThreshold = 3.f);

        void setSampleRate(float sr);
        void setUndersampleFactor(int undersample);

    private:
        float sampleRate;
        int undersampleFactor {10};
        float lookaheadSeconds;

        EnvelopeFollower<float> fastEnv;
        EnvelopeFollower<float> slowEnv;
        EnvelopeFollower<float> lookaheadEnv;

        static EnvelopeFollower<float> getFastEnv(float sensitivity);
        static EnvelopeFollower<float> getSlowEnv(float sensitivity);

        void updateSampleRates();
    };
}
