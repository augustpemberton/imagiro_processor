//
// Preset Tests
// Tests for preset serialization, legacy format conversion, and binary encoding
//

#include <catch2/catch_test_macros.hpp>
#include <juce_gui_basics/juce_gui_basics.h>
#include <imagiro_processor/preset/Preset.h>

using namespace imagiro;

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
    // Create a test preset with a specific gain value for identification
    Preset createTestPreset(const std::string& name, float gainValue) {
        PresetMetadata meta;
        meta.name = name;
        meta.description = "Test preset";

        Preset preset(meta);
        json state;
        state["params"] = json::object();
        state["params"]["gain"] = gainValue;
        preset.state() = state;

        return preset;
    }

    // Create a temporary preset file
    juce::File createTempPresetFile(const Preset& preset, const juce::String& filename) {
        auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        auto presetFile = tempDir.getChildFile(filename);
        preset.saveToFile(presetFile.getFullPathName().toStdString());
        return presetFile;
    }

    // Clean up temp files
    struct TempFileCleanup {
        std::vector<juce::File> files;
        ~TempFileCleanup() {
            for (auto& f : files) {
                f.deleteFile();
            }
        }
        void add(const juce::File& f) { files.push_back(f); }
    };
}

// ============================================================================
// MARK: - Preset Format Tests
// ============================================================================

TEST_CASE("Preset serialization", "[preset][format]") {

    SECTION("New preset saves with version 2") {
        auto preset = createTestPreset("test", 0.0f);
        auto json = preset.toJson();

        REQUIRE(json.contains("version"));
        REQUIRE(json["version"].get<int>() == 2);
    }

    SECTION("Preset round-trips correctly") {
        auto original = createTestPreset("roundtrip", -6.0f);
        auto json = original.toJson();
        auto loaded = Preset::fromJson(json);

        REQUIRE(loaded.has_value());
        REQUIRE(loaded->metadata().name == "roundtrip");
        REQUIRE(loaded->state()["params"]["gain"].get<float>() == -6.0f);
    }

    SECTION("Preset saves and loads from file") {
        TempFileCleanup cleanup;

        auto original = createTestPreset("filetest", 3.0f);
        auto file = createTempPresetFile(original, "test_preset.json");
        cleanup.add(file);

        auto loaded = Preset::loadFromFile(file.getFullPathName().toStdString());

        REQUIRE(loaded.has_value());
        REQUIRE(loaded->metadata().name == "filetest");
        REQUIRE(loaded->state()["params"]["gain"].get<float>() == 3.0f);
    }
}

// ============================================================================
// MARK: - Legacy Preset Conversion Tests
// ============================================================================

TEST_CASE("Legacy preset conversion", "[preset][legacy]") {

    SECTION("Converts legacy format without version field") {
        json legacyJson = {
            {"name", "legacy preset"},
            {"description", "old format"},
            {"paramStates", json::array({
                json::array({"gain", -12.0, "default", 0.0}),
                json::array({"tone", 5.0, "default", 0.0})
            })},
            {"data", {
                {"mappingName", "piano 1"},
                {"velocityCurve", {{"segments", json::array()}}},
                {"tuningMode", 0}
            }}
        };

        auto preset = Preset::fromJson(legacyJson);

        REQUIRE(preset.has_value());
        REQUIRE(preset->metadata().name == "legacy preset");
        REQUIRE(preset->state().contains("params"));
        REQUIRE(preset->state()["params"]["gain"].get<float>() == -12.0f);
        REQUIRE(preset->state()["params"]["tone"].get<float>() == 5.0f);
        REQUIRE(preset->state().contains("samplerData"));
        REQUIRE(preset->state()["samplerData"]["mappingName"] == "piano 1");
    }

    SECTION("Converts legacy paramStates array to params object") {
        json legacyJson = {
            {"name", "param test"},
            {"description", ""},
            {"paramStates", json::array({
                json::array({"attack", 0.1, "default", 0.0}),
                json::array({"decay", 0.4, "default", 0.0}),
                json::array({"sustain", -6.0, "default", 0.0}),
                json::array({"release", 0.5, "default", 0.0})
            })},
            {"data", json::object()}
        };

        auto preset = Preset::fromJson(legacyJson);

        REQUIRE(preset.has_value());
        auto& params = preset->state()["params"];
        REQUIRE(params["attack"].get<float>() == 0.1f);
        REQUIRE(params["decay"].get<float>() == 0.4f);
        REQUIRE(params["sustain"].get<float>() == -6.0f);
        REQUIRE(params["release"].get<float>() == 0.5f);
    }

    SECTION("New format with version 2 loads directly") {
        json newJson = {
            {"version", 2},
            {"metadata", {
                {"name", "new preset"},
                {"description", "new format"}
            }},
            {"state", {
                {"params", {{"gain", 0.0}}},
                {"samplerData", json::object()}
            }}
        };

        auto preset = Preset::fromJson(newJson);

        REQUIRE(preset.has_value());
        REQUIRE(preset->metadata().name == "new preset");
        REQUIRE(preset->state()["params"]["gain"].get<float>() == 0.0f);
    }
}

// ============================================================================
// MARK: - Binary Serialization Tests
// ============================================================================

TEST_CASE("Preset binary serialization", "[preset][binary]") {

    SECTION("Preset round-trips through binary format") {
        auto original = createTestPreset("binary test", -15.0f);

        auto binary = original.toBinary();
        REQUIRE_FALSE(binary.empty());

        auto loaded = Preset::fromBinary(binary.data(), binary.size());
        REQUIRE(loaded.has_value());
        REQUIRE(loaded->metadata().name == "binary test");
        REQUIRE(loaded->state()["params"]["gain"].get<float>() == -15.0f);
    }

    SECTION("Invalid binary returns nullopt") {
        std::vector<uint8_t> garbage = {0x00, 0x01, 0x02, 0x03};
        auto loaded = Preset::fromBinary(garbage.data(), garbage.size());
        REQUIRE_FALSE(loaded.has_value());
    }
}
