#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <juce_audio_basics/juce_audio_basics.h>
#include <imagiro_processor/processor/BypassMixer.h>

#include <cmath>
#include <limits>

using namespace imagiro;
using Catch::Matchers::WithinAbs;

TEST_CASE("BypassMixer ignores non-finite dry samples when fully wet", "[processor][bypass]") {
    BypassMixer mixer;
    mixer.prepare(48000.0, 2);
    mixer.setBypass(false);
    mixer.setMix(1.f);
    mixer.skipSmoothing();

    juce::AudioSampleBuffer buffer(2, 16);
    buffer.clear();
    for (int c = 0; c < buffer.getNumChannels(); c++)
        for (int s = 0; s < buffer.getNumSamples(); s++)
            buffer.setSample(c, s, std::numeric_limits<float>::quiet_NaN());

    mixer.pushDry(buffer);

    for (int c = 0; c < buffer.getNumChannels(); c++)
        for (int s = 0; s < buffer.getNumSamples(); s++)
            buffer.setSample(c, s, c == 0 ? 0.25f : -0.125f);

    mixer.applyMix(buffer);

    for (int c = 0; c < buffer.getNumChannels(); c++) {
        const float expected = c == 0 ? 0.25f : -0.125f;
        for (int s = 0; s < buffer.getNumSamples(); s++) {
            const auto sample = buffer.getSample(c, s);
            REQUIRE(std::isfinite(sample));
            REQUIRE_THAT(sample, WithinAbs(expected, 0.000001f));
        }
    }
}

TEST_CASE("BypassMixer does not emit non-finite wet samples", "[processor][bypass]") {
    BypassMixer mixer;
    mixer.prepare(48000.0, 2);
    mixer.setBypass(false);
    mixer.setMix(1.f);
    mixer.skipSmoothing();

    juce::AudioSampleBuffer buffer(2, 16);
    buffer.clear();
    mixer.pushDry(buffer);

    for (int c = 0; c < buffer.getNumChannels(); c++)
        for (int s = 0; s < buffer.getNumSamples(); s++)
            buffer.setSample(c, s, std::numeric_limits<float>::infinity());

    mixer.applyMix(buffer);

    for (int c = 0; c < buffer.getNumChannels(); c++) {
        for (int s = 0; s < buffer.getNumSamples(); s++) {
            const auto sample = buffer.getSample(c, s);
            REQUIRE(std::isfinite(sample));
            REQUIRE_THAT(sample, WithinAbs(0.f, 0.000001f));
        }
    }
}
