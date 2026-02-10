//
// ParamController Tests
// Tests for thread-safe parameter management
//

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <imagiro_util/util.h>
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
// MARK: - Test Helpers
// ============================================================================

namespace {
    ParamConfig makeLinearParam(const std::string& uid, float min, float max, float defaultVal) {
        return {
            .uid = uid,
            .name = uid,
            .range = ParamRange::linear(min, max),
            .format = ValueFormatter::number(),
            .defaultValue = defaultVal
        };
    }

    ParamConfig makeToggleParam(const std::string& uid, bool defaultVal) {
        return {
            .uid = uid,
            .name = uid,
            .range = ParamRange::toggle(),
            .format = ValueFormatter::toggle(),
            .defaultValue = defaultVal ? 1.f : 0.f
        };
    }
}

// ============================================================================
// MARK: - Basic Parameter Operations
// ============================================================================

TEST_CASE("ParamController basic operations", "[param][controller]") {

    SECTION("Can add and retrieve parameters") {
        ParamController ctrl;
        auto h = ctrl.addParam(makeLinearParam("gain", -60.f, 12.f, 0.f));

        REQUIRE(ctrl.size() == 1);
        REQUIRE(ctrl.has("gain"));
        REQUIRE(ctrl.handle("gain").index == h.index);
    }

    SECTION("Can add multiple parameters") {
        ParamController ctrl;
        ctrl.addParam(makeLinearParam("gain", -60.f, 12.f, 0.f));
        ctrl.addParam(makeLinearParam("pan", 0.f, 1.f, 0.5f));
        ctrl.addParam(makeToggleParam("bypass", false));

        REQUIRE(ctrl.size() == 3);
        REQUIRE(ctrl.has("gain"));
        REQUIRE(ctrl.has("pan"));
        REQUIRE(ctrl.has("bypass"));
    }

    SECTION("has() returns false for unknown uid") {
        ParamController ctrl;
        ctrl.addParam(makeLinearParam("gain", -60.f, 12.f, 0.f));

        REQUIRE_FALSE(ctrl.has("unknown"));
    }

    SECTION("Config is accessible via handle") {
        ParamController ctrl;
        auto h = ctrl.addParam(makeLinearParam("gain", -60.f, 12.f, 0.f));

        const auto& cfg = ctrl.config(h);
        REQUIRE(cfg.uid == "gain");
        REQUIRE_THAT(cfg.defaultValue, WithinAbs(0.0, 0.0001));
    }
}

// ============================================================================
// MARK: - Default Values
// ============================================================================

TEST_CASE("ParamController default values", "[param][controller]") {

    SECTION("Parameter initializes to default value") {
        ParamController ctrl;
        auto h = ctrl.addParam(makeLinearParam("gain", -60.f, 12.f, -6.f));

        REQUIRE_THAT(ctrl.getValueUI(h), WithinAbs(-6.0, 0.0001));
    }

    SECTION("Default value is clamped to range") {
        ParamController ctrl;
        // Default of 100 should be clamped to max of 12
        auto h = ctrl.addParam(makeLinearParam("gain", -60.f, 12.f, 100.f));

        REQUIRE_THAT(ctrl.getValueUI(h), WithinAbs(12.0, 0.0001));
    }

    SECTION("resetToDefault restores default value") {
        ParamController ctrl;
        auto h = ctrl.addParam(makeLinearParam("gain", -60.f, 12.f, 0.f));

        ctrl.setValue(h, -12.f);
        ctrl.resetToDefault(h);

        REQUIRE_THAT(ctrl.getValueUI(h), WithinAbs(0.0, 0.0001));
    }
}

// ============================================================================
// MARK: - setValue / getValue
// ============================================================================

TEST_CASE("ParamController setValue/getValue", "[param][controller]") {

    SECTION("setValue updates value") {
        ParamController ctrl;
        auto h = ctrl.addParam(makeLinearParam("gain", -60.f, 12.f, 0.f));

        ctrl.setValue(h, -12.f);

        REQUIRE_THAT(ctrl.getValueUI(h), WithinAbs(-12.0, 0.0001));
    }

    SECTION("setValue clamps to range") {
        ParamController ctrl;
        auto h = ctrl.addParam(makeLinearParam("gain", -60.f, 12.f, 0.f));

        ctrl.setValue(h, 100.f);
        REQUIRE_THAT(ctrl.getValueUI(h), WithinAbs(12.0, 0.0001));

        ctrl.setValue(h, -100.f);
        REQUIRE_THAT(ctrl.getValueUI(h), WithinAbs(-60.0, 0.0001));
    }

    SECTION("setValue01 sets normalized value") {
        ParamController ctrl;
        auto h = ctrl.addParam(makeLinearParam("gain", 0.f, 100.f, 0.f));

        ctrl.setValue01(h, 0.5f);

        REQUIRE_THAT(ctrl.getValueUI(h), WithinAbs(50.0, 0.0001));
        REQUIRE_THAT(ctrl.getValue01UI(h), WithinAbs(0.5, 0.0001));
    }

    SECTION("setValue01 clamps to 0-1") {
        ParamController ctrl;
        auto h = ctrl.addParam(makeLinearParam("gain", 0.f, 100.f, 0.f));

        ctrl.setValue01(h, 1.5f);

        REQUIRE_THAT(ctrl.getValue01UI(h), WithinAbs(1.0, 0.0001));
    }

    SECTION("Pending value is visible before captureAudio") {
        ParamController ctrl;
        auto h = ctrl.addParam(makeLinearParam("gain", 0.f, 100.f, 0.f));

        ctrl.setValue(h, 50.f);

        // Value should be visible immediately on UI thread
        REQUIRE_THAT(ctrl.getValueUI(h), WithinAbs(50.0, 0.0001));
    }
}

// ============================================================================
// MARK: - Text Formatting
// ============================================================================

TEST_CASE("ParamController text formatting", "[param][controller]") {

    SECTION("getValueTextUI returns formatted string") {
        ParamController ctrl;
        auto h = ctrl.addParam({
            .uid = "gain",
            .name = "gain",
            .range = ParamRange::linear(-60.f, 12.f),
            .format = ValueFormatter::decibels(),
            .defaultValue = -12.f
        });

        auto text = ctrl.getValueTextUI(h);
        REQUIRE(text.find("dB") != std::string::npos);
        REQUIRE(text.find("-12") != std::string::npos);
    }

    SECTION("setValueFromTextUI parses and sets value") {
        ParamController ctrl;
        auto h = ctrl.addParam({
            .uid = "gain",
            .name = "gain",
            .range = ParamRange::linear(-60.f, 12.f),
            .format = ValueFormatter::decibels(),
            .defaultValue = 0.f
        });

        bool success = ctrl.setValueFromTextUI(h, "-6dB");

        REQUIRE(success);
        REQUIRE_THAT(ctrl.getValueUI(h), WithinAbs(-6.0, 0.0001));
    }

    SECTION("setValueFromTextUI returns false for invalid input") {
        ParamController ctrl;
        auto h = ctrl.addParam(makeLinearParam("gain", -60.f, 12.f, 0.f));

        bool success = ctrl.setValueFromTextUI(h, "invalid");

        REQUIRE_FALSE(success);
    }
}

// ============================================================================
// MARK: - forEach
// ============================================================================

TEST_CASE("ParamController forEach", "[param][controller]") {

    SECTION("Iterates all parameters") {
        ParamController ctrl;
        ctrl.addParam(makeLinearParam("a", 0.f, 1.f, 0.f));
        ctrl.addParam(makeLinearParam("b", 0.f, 1.f, 0.f));
        ctrl.addParam(makeLinearParam("c", 0.f, 1.f, 0.f));

        std::vector<std::string> uids;
        ctrl.forEach([&](Handle h, const ParamConfig& cfg) {
            uids.push_back(cfg.uid);
        });

        REQUIRE(uids.size() == 3);
        REQUIRE(uids[0] == "a");
        REQUIRE(uids[1] == "b");
        REQUIRE(uids[2] == "c");
    }
}

// ============================================================================
// MARK: - State Registry
// ============================================================================

TEST_CASE("ParamController state registry", "[param][controller]") {

    SECTION("registryUI returns current state") {
        ParamController ctrl;
        auto h = ctrl.addParam(makeLinearParam("gain", 0.f, 100.f, 50.f));

        auto reg = ctrl.registryUI();

        REQUIRE_THAT(reg.get(h).userValue, WithinAbs(50.0, 0.0001));
    }

    SECTION("setRegistryUI loads state") {
        ParamController ctrl;
        auto h = ctrl.addParam(makeLinearParam("gain", 0.f, 100.f, 0.f));

        // Get registry, modify it, set it back
        auto reg = ctrl.registryUI();
        auto val = reg.get(h);
        val.userValue = 75.f;
        val.value01 = 0.75f;
        reg = reg.set(h, val);

        ctrl.setRegistryUI(reg);

        REQUIRE_THAT(ctrl.getValueUI(h), WithinAbs(75.0, 0.0001));
    }

    SECTION("setRegistryUI clamps values to current range") {
        ParamController ctrl;
        auto h = ctrl.addParam(makeLinearParam("gain", 0.f, 100.f, 0.f));

        auto reg = ctrl.registryUI();
        auto val = reg.get(h);
        val.userValue = 200.f;  // Out of range
        reg = reg.set(h, val);

        ctrl.setRegistryUI(reg);

        REQUIRE_THAT(ctrl.getValueUI(h), WithinAbs(100.0, 0.0001));
    }
}

// ============================================================================
// MARK: - Audio Thread Capture
// ============================================================================

TEST_CASE("ParamController audio capture", "[param][controller]") {

    SECTION("snapshotInto returns current state") {
        ParamController ctrl;
        auto h = ctrl.addParam(makeLinearParam("gain", 0.f, 100.f, 50.f));

        std::vector<ParamValue> out(ctrl.size());
        ctrl.snapshotInto(out);

        REQUIRE_THAT(out[h.index].userValue, WithinAbs(50.0, 0.0001));
    }

    SECTION("snapshotInto reflects setValue immediately") {
        ParamController ctrl;
        auto h = ctrl.addParam(makeLinearParam("gain", 0.f, 100.f, 0.f));

        ctrl.setValue(h, 75.f);

        std::vector<ParamValue> out(ctrl.size());
        ctrl.snapshotInto(out);

        REQUIRE_THAT(out[h.index].userValue, WithinAbs(75.0, 0.0001));
    }

    SECTION("Multiple rapid changes only final value persists") {
        ParamController ctrl;
        auto h = ctrl.addParam(makeLinearParam("gain", 0.f, 100.f, 0.f));

        ctrl.setValue(h, 25.f);
        ctrl.setValue(h, 50.f);
        ctrl.setValue(h, 75.f);

        std::vector<ParamValue> out(ctrl.size());
        ctrl.snapshotInto(out);

        REQUIRE_THAT(out[h.index].userValue, WithinAbs(75.0, 0.0001));
    }
}
