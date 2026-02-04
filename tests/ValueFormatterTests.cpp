//
// ValueFormatter Tests
// Tests for parameter value formatting and parsing
//

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <imagiro_util/util.h>
#include <imagiro_processor/parameter/ParamRange.h>
#include <imagiro_processor/parameter/ValueFormatter.h>

using namespace imagiro;
using Catch::Matchers::WithinAbs;

// ============================================================================
// MARK: - Number Formatter Tests
// ============================================================================

TEST_CASE("ValueFormatter number", "[param][formatter]") {

    SECTION("Formats with appropriate precision based on digits param") {
        auto fmt = ValueFormatter::number(3);
        // The formatter uses dynamic decimal places based on total significant digits
        auto str5 = fmt.toString(5.f);
        REQUIRE(str5.find("5") != std::string::npos);

        auto str100 = fmt.toString(100.f);
        REQUIRE(str100.find("100") != std::string::npos);
    }

    SECTION("Formats decimals") {
        auto fmt = ValueFormatter::number(3);
        auto str = fmt.toString(1.5f);
        REQUIRE(str.find("1.5") != std::string::npos);
    }

    SECTION("Parses valid numbers") {
        auto fmt = ValueFormatter::number();
        REQUIRE(fmt.fromString("42").has_value());
        REQUIRE_THAT(*fmt.fromString("42"), WithinAbs(42.0, 0.0001));
        REQUIRE_THAT(*fmt.fromString("-3.14"), WithinAbs(-3.14, 0.001));
    }

    SECTION("Returns nullopt for invalid input") {
        auto fmt = ValueFormatter::number();
        REQUIRE_FALSE(fmt.fromString("abc").has_value());
        REQUIRE_FALSE(fmt.fromString("").has_value());
    }

    SECTION("Sanitizes very small values to zero") {
        auto fmt = ValueFormatter::number(3);
        auto str = fmt.toString(0.0000001f);
        REQUIRE(str.find("0") != std::string::npos);
    }
}

// ============================================================================
// MARK: - Suffix Formatter Tests
// ============================================================================

TEST_CASE("ValueFormatter withSuffix", "[param][formatter]") {

    SECTION("Appends suffix to value") {
        auto fmt = ValueFormatter::withSuffix("st", 2);
        auto str12 = fmt.toString(12.f);
        REQUIRE(str12.find("12") != std::string::npos);
        REQUIRE(str12.find("st") != std::string::npos);
    }

    SECTION("Parses ignoring suffix") {
        auto fmt = ValueFormatter::withSuffix("st");
        REQUIRE_THAT(*fmt.fromString("12st"), WithinAbs(12.0, 0.0001));
        REQUIRE_THAT(*fmt.fromString("12"), WithinAbs(12.0, 0.0001));
    }
}

// ============================================================================
// MARK: - Decibels Formatter Tests
// ============================================================================

TEST_CASE("ValueFormatter decibels", "[param][formatter]") {

    SECTION("Formats with dB suffix") {
        auto fmt = ValueFormatter::decibels();
        auto str0 = fmt.toString(0.f);
        REQUIRE(str0.find("0") != std::string::npos);
        REQUIRE(str0.find("dB") != std::string::npos);

        auto str12 = fmt.toString(-12.f);
        REQUIRE(str12.find("-12") != std::string::npos);
        REQUIRE(str12.find("dB") != std::string::npos);
    }

    SECTION("Parses dB values") {
        auto fmt = ValueFormatter::decibels();
        REQUIRE_THAT(*fmt.fromString("-12dB"), WithinAbs(-12.0, 0.0001));
        REQUIRE_THAT(*fmt.fromString("-12"), WithinAbs(-12.0, 0.0001));
        REQUIRE_THAT(*fmt.fromString("0dB"), WithinAbs(0.0, 0.0001));
    }
}

// ============================================================================
// MARK: - Frequency Formatter Tests
// ============================================================================

TEST_CASE("ValueFormatter frequency", "[param][formatter]") {

    SECTION("Formats Hz for values < 1000") {
        auto fmt = ValueFormatter::frequency();
        REQUIRE(fmt.toString(440.f).find("Hz") != std::string::npos);
        REQUIRE(fmt.toString(440.f).find("kHz") == std::string::npos);
    }

    SECTION("Formats kHz for values >= 1000") {
        auto fmt = ValueFormatter::frequency();
        auto str = fmt.toString(1500.f);
        REQUIRE(str.find("kHz") != std::string::npos);
        REQUIRE(str.find("1.5") != std::string::npos);
    }

    SECTION("Parses Hz values") {
        auto fmt = ValueFormatter::frequency();
        REQUIRE_THAT(*fmt.fromString("440 Hz"), WithinAbs(440.0, 0.1));
        REQUIRE_THAT(*fmt.fromString("440"), WithinAbs(440.0, 0.1));
    }

    SECTION("Parses kHz values with multiplier") {
        auto fmt = ValueFormatter::frequency();
        REQUIRE_THAT(*fmt.fromString("1.5 kHz"), WithinAbs(1500.0, 1.0));
        REQUIRE_THAT(*fmt.fromString("2k"), WithinAbs(2000.0, 1.0));
    }
}

// ============================================================================
// MARK: - Time Formatter Tests
// ============================================================================

TEST_CASE("ValueFormatter time", "[param][formatter]") {

    SECTION("Formats ms for values < 1 second") {
        auto fmt = ValueFormatter::time();
        auto str = fmt.toString(0.150f);
        REQUIRE(str.find("ms") != std::string::npos);
        REQUIRE(str.find("150") != std::string::npos);
    }

    SECTION("Formats s for values >= 1 second") {
        auto fmt = ValueFormatter::time();
        auto str = fmt.toString(2.5f);
        REQUIRE(str.find(" s") != std::string::npos);
        REQUIRE(str.find("2.5") != std::string::npos);
    }

    SECTION("Parses ms values") {
        auto fmt = ValueFormatter::time();
        REQUIRE_THAT(*fmt.fromString("150 ms"), WithinAbs(0.15, 0.001));
        REQUIRE_THAT(*fmt.fromString("150ms"), WithinAbs(0.15, 0.001));
    }

    SECTION("Parses s values") {
        auto fmt = ValueFormatter::time();
        REQUIRE_THAT(*fmt.fromString("2.5 s"), WithinAbs(2.5, 0.001));
        REQUIRE_THAT(*fmt.fromString("2.5"), WithinAbs(2.5, 0.001));
    }
}

// ============================================================================
// MARK: - Percent Formatter Tests
// ============================================================================

TEST_CASE("ValueFormatter percent", "[param][formatter]") {

    SECTION("Formats 0-1 as percentage") {
        auto fmt = ValueFormatter::percent();
        auto str50 = fmt.toString(0.5f);
        REQUIRE(str50.find("50") != std::string::npos);
        REQUIRE(str50.find("%") != std::string::npos);
    }

    SECTION("Parses percentage to 0-1") {
        auto fmt = ValueFormatter::percent();
        REQUIRE_THAT(*fmt.fromString("50%"), WithinAbs(0.5, 0.0001));
        REQUIRE_THAT(*fmt.fromString("100%"), WithinAbs(1.0, 0.0001));
        REQUIRE_THAT(*fmt.fromString("0%"), WithinAbs(0.0, 0.0001));
    }

    SECTION("Handles values over 100%") {
        auto fmt = ValueFormatter::percent();
        REQUIRE_THAT(*fmt.fromString("150%"), WithinAbs(1.5, 0.0001));
    }
}

// ============================================================================
// MARK: - Toggle Formatter Tests
// ============================================================================

TEST_CASE("ValueFormatter toggle", "[param][formatter]") {

    SECTION("Formats as on/off") {
        auto fmt = ValueFormatter::toggle();
        REQUIRE(fmt.toString(1.f) == "on");
        REQUIRE(fmt.toString(0.f) == "off");
        REQUIRE(fmt.toString(0.6f) == "on");
        REQUIRE(fmt.toString(0.4f) == "off");
    }

    SECTION("Parses on/off strings") {
        auto fmt = ValueFormatter::toggle();
        REQUIRE_THAT(*fmt.fromString("on"), WithinAbs(1.0, 0.0001));
        REQUIRE_THAT(*fmt.fromString("On"), WithinAbs(1.0, 0.0001));
        REQUIRE_THAT(*fmt.fromString("off"), WithinAbs(0.0, 0.0001));
        REQUIRE_THAT(*fmt.fromString("Off"), WithinAbs(0.0, 0.0001));
    }

    SECTION("Parses boolean strings") {
        auto fmt = ValueFormatter::toggle();
        REQUIRE_THAT(*fmt.fromString("true"), WithinAbs(1.0, 0.0001));
        REQUIRE_THAT(*fmt.fromString("false"), WithinAbs(0.0, 0.0001));
        REQUIRE_THAT(*fmt.fromString("1"), WithinAbs(1.0, 0.0001));
        REQUIRE_THAT(*fmt.fromString("0"), WithinAbs(0.0, 0.0001));
    }

    SECTION("Returns nullopt for invalid input") {
        auto fmt = ValueFormatter::toggle();
        REQUIRE_FALSE(fmt.fromString("maybe").has_value());
    }
}

// ============================================================================
// MARK: - Choice Formatter Tests
// ============================================================================

TEST_CASE("ValueFormatter choice", "[param][formatter]") {

    SECTION("Formats index as choice string") {
        auto fmt = ValueFormatter::choice({"low", "medium", "high"});
        REQUIRE(fmt.toString(0.f) == "low");
        REQUIRE(fmt.toString(1.f) == "medium");
        REQUIRE(fmt.toString(2.f) == "high");
    }

    SECTION("Rounds float index") {
        auto fmt = ValueFormatter::choice({"a", "b", "c"});
        REQUIRE(fmt.toString(0.4f) == "a");
        REQUIRE(fmt.toString(0.6f) == "b");
        REQUIRE(fmt.toString(1.5f) == "c");  // 1.5 rounds to 2 -> "c"
    }

    SECTION("Returns ? for out of range") {
        auto fmt = ValueFormatter::choice({"a", "b"});
        REQUIRE(fmt.toString(5.f) == "?");
        REQUIRE(fmt.toString(-1.f) == "?");
    }

    SECTION("Parses choice string to index") {
        auto fmt = ValueFormatter::choice({"low", "medium", "high"});
        REQUIRE_THAT(*fmt.fromString("low"), WithinAbs(0.0, 0.0001));
        REQUIRE_THAT(*fmt.fromString("medium"), WithinAbs(1.0, 0.0001));
        REQUIRE_THAT(*fmt.fromString("high"), WithinAbs(2.0, 0.0001));
    }

    SECTION("Returns nullopt for unknown choice") {
        auto fmt = ValueFormatter::choice({"a", "b", "c"});
        REQUIRE_FALSE(fmt.fromString("d").has_value());
    }
}

// ============================================================================
// MARK: - Pan Formatter Tests
// ============================================================================

TEST_CASE("ValueFormatter pan", "[param][formatter]") {

    SECTION("Formats center as C") {
        auto fmt = ValueFormatter::pan();
        auto str = fmt.toString(0.5f);
        REQUIRE(str.find("C") != std::string::npos);
    }

    SECTION("Formats left with L suffix") {
        auto fmt = ValueFormatter::pan();
        auto str = fmt.toString(0.f);
        REQUIRE(str.find("L") != std::string::npos);
    }

    SECTION("Formats right with R suffix") {
        auto fmt = ValueFormatter::pan();
        auto str = fmt.toString(1.f);
        REQUIRE(str.find("R") != std::string::npos);
    }

    SECTION("Parses pan strings") {
        auto fmt = ValueFormatter::pan();
        REQUIRE_THAT(*fmt.fromString("50L"), WithinAbs(0.0, 0.01));
        REQUIRE_THAT(*fmt.fromString("50R"), WithinAbs(1.0, 0.01));
        REQUIRE_THAT(*fmt.fromString("0C"), WithinAbs(0.5, 0.01));
    }
}

// ============================================================================
// MARK: - Note Division Formatter Tests
// ============================================================================

TEST_CASE("ValueFormatter noteDivision", "[param][formatter]") {

    SECTION("Formats fractions correctly") {
        auto fmt = ValueFormatter::noteDivision();
        REQUIRE(fmt.toString(-1.f) == "1/2");
        REQUIRE(fmt.toString(-2.f) == "1/4");
        REQUIRE(fmt.toString(-3.f) == "1/8");
        REQUIRE(fmt.toString(-4.f) == "1/16");
    }

    SECTION("Formats whole notes correctly") {
        auto fmt = ValueFormatter::noteDivision();
        REQUIRE(fmt.toString(0.f) == "1");
        REQUIRE(fmt.toString(1.f) == "2");
        REQUIRE(fmt.toString(2.f) == "4");
        REQUIRE(fmt.toString(3.f) == "8");
    }

    SECTION("Parses fractions") {
        auto fmt = ValueFormatter::noteDivision();
        REQUIRE_THAT(*fmt.fromString("1/2"), WithinAbs(-1.0, 0.0001));
        REQUIRE_THAT(*fmt.fromString("1/4"), WithinAbs(-2.0, 0.0001));
        REQUIRE_THAT(*fmt.fromString("1/8"), WithinAbs(-3.0, 0.0001));
        REQUIRE_THAT(*fmt.fromString("1/16"), WithinAbs(-4.0, 0.0001));
    }

    SECTION("Parses whole notes") {
        auto fmt = ValueFormatter::noteDivision();
        REQUIRE_THAT(*fmt.fromString("1"), WithinAbs(0.0, 0.0001));
        REQUIRE_THAT(*fmt.fromString("2"), WithinAbs(1.0, 0.0001));
        REQUIRE_THAT(*fmt.fromString("4"), WithinAbs(2.0, 0.0001));
    }

    SECTION("Round-trips correctly") {
        auto fmt = ValueFormatter::noteDivision();

        for (float v : {-4.f, -3.f, -2.f, -1.f, 0.f, 1.f, 2.f, 3.f}) {
            auto str = fmt.toString(v);
            auto parsed = fmt.fromString(str);
            REQUIRE(parsed.has_value());
            REQUIRE_THAT(*parsed, WithinAbs(v, 0.0001));
        }
    }
}
