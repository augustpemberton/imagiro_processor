//
// ProcessState Tests
// Tests for audio thread state capture and processor transforms
//

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <imagiro_util/util.h>
#include <imagiro_processor/parameter/ParamValue.h>
#include <imagiro_processor/processor/state/ProcessState.h>
#include <imagiro_processor/parameter/ParamController.h>
#include <imagiro_processor/parameter/ParamConfig.h>

using namespace imagiro;
using Catch::Matchers::WithinAbs;

// ============================================================================
// MARK: - JUCE Initialization
// ============================================================================

namespace {
    void initJuceForTests() {
        static bool initialized = false;
        if (!initialized) {
            juce::MessageManager::getInstance();
            initialized = true;
        }
    }

    struct JuceTestInit {
        JuceTestInit() { initJuceForTests(); }
    };

    JuceTestInit juceInit;
}

// ============================================================================
// MARK: - Basic ProcessState Tests
// ============================================================================

TEST_CASE("ProcessState basic operations", "[state]") {

    SECTION("Default state has sensible defaults") {
        ProcessState state;

        // Defaults: 120 BPM, 44100 sample rate
        REQUIRE_THAT(state.bpm(), WithinAbs(120.0, 0.0001));
        REQUIRE_THAT(state.sampleRate(), WithinAbs(44100.0, 0.0001));
    }

    SECTION("Can set and get BPM") {
        ProcessState state;
        state.setBpm(140.0);

        REQUIRE_THAT(state.bpm(), WithinAbs(140.0, 0.0001));
    }

    SECTION("Can set and get sample rate") {
        ProcessState state;
        state.setSampleRate(48000.0);

        REQUIRE_THAT(state.sampleRate(), WithinAbs(48000.0, 0.0001));
    }
}

// ============================================================================
// MARK: - Parameter Value Tests
// ============================================================================

TEST_CASE("ProcessState parameter values", "[state]") {

    SECTION("Can get raw user value") {
        ParamController ctrl;
        auto h = ctrl.addParam({
            .uid = "gain",
            .name = "gain",
            .range = ParamRange::linear(-60.f, 12.f),
            .format = ValueFormatter::number(),
            .defaultValue = -12.f
        });

        ProcessState state;
        state.params() = ctrl.captureAudio();

        REQUIRE_THAT(state.userValue(h), WithinAbs(-12.0, 0.0001));
    }

    SECTION("value() returns userValue when no toProcessor transform") {
        ParamController ctrl;
        auto h = ctrl.addParam({
            .uid = "gain",
            .name = "gain",
            .range = ParamRange::linear(-60.f, 12.f),
            .format = ValueFormatter::number(),
            .defaultValue = -12.f
        });

        ProcessState state;
        state.params() = ctrl.captureAudio();

        // Without toProcessor, value() should equal userValue()
        REQUIRE_THAT(state.value(h), WithinAbs(-12.0, 0.0001));
    }

    SECTION("value() applies toProcessor transform") {
        ParamController ctrl;
        auto h = ctrl.addParam({
            .uid = "gain",
            .name = "gain",
            .range = ParamRange::linear(-60.f, 12.f),
            .format = ValueFormatter::decibels(),
            .toProcessor = +[](float db, double, double) {
                return std::pow(10.f, db / 20.f);  // dB to linear
            },
            .defaultValue = -20.f  // -20dB = 0.1 linear
        });

        ProcessState state;
        state.setSampleRate(48000.0);
        state.params() = ctrl.captureAudio();

        // -20dB should convert to 0.1 linear
        REQUIRE_THAT(state.value(h), WithinAbs(0.1, 0.001));
    }

    SECTION("toProcessor is called when set") {
        ParamController ctrl;
        auto h = ctrl.addParam({
            .uid = "db",
            .name = "db",
            .range = ParamRange::linear(-60.f, 12.f),
            .format = ValueFormatter::decibels(),
            .toProcessor = +[](float db, double, double) {
                // Convert dB to linear gain: 10^(db/20)
                return std::pow(10.f, db / 20.f);
            },
            .defaultValue = -20.f  // -20dB = 0.1 linear
        });

        ProcessState state;
        state.params() = ctrl.captureAudio();

        // -20dB should convert to 0.1 linear (10^(-20/20) = 10^-1 = 0.1)
        float result = state.value(h);
        REQUIRE_THAT(result, WithinAbs(0.1, 0.01));
    }

    SECTION("toProcessor returns userValue when not set") {
        ParamController ctrl;
        auto h = ctrl.addParam({
            .uid = "raw",
            .name = "raw",
            .range = ParamRange::linear(0.f, 100.f),
            .format = ValueFormatter::number(),
            // No toProcessor set
            .defaultValue = 42.f
        });

        ProcessState state;
        state.params() = ctrl.captureAudio();

        // Without toProcessor, value() should return userValue
        REQUIRE_THAT(state.value(h), WithinAbs(42.0, 0.0001));
        REQUIRE_THAT(state.userValue(h), WithinAbs(42.0, 0.0001));
    }
}

// ============================================================================
// MARK: - Normalized Value Tests
// ============================================================================

TEST_CASE("ProcessState normalized values", "[state]") {

    SECTION("value01() returns normalized value") {
        ParamController ctrl;
        auto h = ctrl.addParam({
            .uid = "pan",
            .name = "pan",
            .range = ParamRange::linear(0.f, 1.f),
            .format = ValueFormatter::number(),
            .defaultValue = 0.5f
        });

        ProcessState state;
        state.params() = ctrl.captureAudio();

        REQUIRE_THAT(state.value01(h), WithinAbs(0.5, 0.0001));
    }

    SECTION("Normalized value reflects actual range position") {
        ParamController ctrl;
        auto h = ctrl.addParam({
            .uid = "gain",
            .name = "gain",
            .range = ParamRange::linear(-60.f, 0.f),
            .format = ValueFormatter::number(),
            .defaultValue = -30.f  // Midpoint
        });

        ProcessState state;
        state.params() = ctrl.captureAudio();

        REQUIRE_THAT(state.value01(h), WithinAbs(0.5, 0.0001));
    }
}

// ============================================================================
// MARK: - State Immutability Tests
// ============================================================================

TEST_CASE("ProcessState immutability", "[state]") {

    SECTION("Captured state is a snapshot") {
        ParamController ctrl;
        auto h = ctrl.addParam({
            .uid = "gain",
            .name = "gain",
            .range = ParamRange::linear(0.f, 100.f),
            .format = ValueFormatter::number(),
            .defaultValue = 50.f
        });

        ProcessState state;
        state.params() = ctrl.captureAudio();

        // Modify controller after capture
        ctrl.setValue(h, 75.f);

        // State should still have old value
        REQUIRE_THAT(state.userValue(h), WithinAbs(50.0, 0.0001));

        // New capture should have new value
        ctrl.captureAudio();
        ProcessState state2;
        state2.params() = ctrl.captureAudio();
        REQUIRE_THAT(state2.userValue(h), WithinAbs(75.0, 0.0001));
    }

    SECTION("Multiple states can coexist") {
        ParamController ctrl;
        auto h = ctrl.addParam({
            .uid = "gain",
            .name = "gain",
            .range = ParamRange::linear(0.f, 100.f),
            .format = ValueFormatter::number(),
            .defaultValue = 0.f
        });

        ctrl.setValue(h, 25.f);
        ProcessState state1;
        state1.params() = ctrl.captureAudio();

        ctrl.setValue(h, 50.f);
        ProcessState state2;
        state2.params() = ctrl.captureAudio();

        ctrl.setValue(h, 75.f);
        ProcessState state3;
        state3.params() = ctrl.captureAudio();

        // All three states should retain their values
        REQUIRE_THAT(state1.userValue(h), WithinAbs(25.0, 0.0001));
        REQUIRE_THAT(state2.userValue(h), WithinAbs(50.0, 0.0001));
        REQUIRE_THAT(state3.userValue(h), WithinAbs(75.0, 0.0001));
    }
}
