//
// ParamRange Tests
// Tests for parameter range normalization, denormalization, and snapping
//

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <imagiro_processor/parameter/ParamRange.h>

using namespace imagiro;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

// ============================================================================
// MARK: - Linear Range Tests
// ============================================================================

TEST_CASE("ParamRange linear", "[param][range]") {

    SECTION("Basic linear range normalizes correctly") {
        auto range = ParamRange::linear(0.f, 100.f);

        REQUIRE_THAT(range.normalize(0.f), WithinAbs(0.0, 0.0001));
        REQUIRE_THAT(range.normalize(50.f), WithinAbs(0.5, 0.0001));
        REQUIRE_THAT(range.normalize(100.f), WithinAbs(1.0, 0.0001));
    }

    SECTION("Basic linear range denormalizes correctly") {
        auto range = ParamRange::linear(0.f, 100.f);

        REQUIRE_THAT(range.denormalize(0.f), WithinAbs(0.0, 0.0001));
        REQUIRE_THAT(range.denormalize(0.5f), WithinAbs(50.0, 0.0001));
        REQUIRE_THAT(range.denormalize(1.f), WithinAbs(100.0, 0.0001));
    }

    SECTION("Linear range round-trips correctly") {
        auto range = ParamRange::linear(-50.f, 50.f);

        for (float v : {-50.f, -25.f, 0.f, 25.f, 50.f}) {
            float normalized = range.normalize(v);
            float denormalized = range.denormalize(normalized);
            REQUIRE_THAT(denormalized, WithinAbs(v, 0.0001));
        }
    }

    SECTION("Negative range works correctly") {
        auto range = ParamRange::linear(-100.f, -10.f);

        REQUIRE_THAT(range.normalize(-100.f), WithinAbs(0.0, 0.0001));
        REQUIRE_THAT(range.normalize(-55.f), WithinAbs(0.5, 0.0001));
        REQUIRE_THAT(range.normalize(-10.f), WithinAbs(1.0, 0.0001));
    }
}

// ============================================================================
// MARK: - Skewed Range Tests
// ============================================================================

TEST_CASE("ParamRange skewed", "[param][range]") {

    SECTION("Skew < 1 pulls midpoint toward low end") {
        auto range = ParamRange::skewed(0.f, 100.f, 0.5f);

        // With skew 0.5, normalized 0.5 maps to lower than 50
        float mid = range.denormalize(0.5f);
        REQUIRE(mid < 50.f);
    }

    SECTION("Skew > 1 pulls midpoint toward high end") {
        auto range = ParamRange::skewed(0.f, 100.f, 2.f);

        // With skew 2, normalized 0.5 maps to higher than 50
        float mid = range.denormalize(0.5f);
        REQUIRE(mid > 50.f);
    }

    SECTION("Skew = 1 is linear") {
        auto skewed = ParamRange::skewed(0.f, 100.f, 1.f);
        auto linear = ParamRange::linear(0.f, 100.f);

        for (float n = 0.f; n <= 1.f; n += 0.1f) {
            REQUIRE_THAT(skewed.denormalize(n), WithinAbs(linear.denormalize(n), 0.0001));
        }
    }

    SECTION("Skewed range round-trips correctly") {
        auto range = ParamRange::skewed(0.f, 1000.f, 0.3f);

        for (float v : {0.f, 100.f, 500.f, 1000.f}) {
            float normalized = range.normalize(v);
            float denormalized = range.denormalize(normalized);
            REQUIRE_THAT(denormalized, WithinAbs(v, 0.01));
        }
    }
}

// ============================================================================
// MARK: - Logarithmic Range Tests
// ============================================================================

TEST_CASE("ParamRange logarithmic", "[param][range]") {

    SECTION("Frequency range covers audio spectrum") {
        auto range = ParamRange::frequency(20.f, 20000.f);

        REQUIRE_THAT(range.denormalize(0.f), WithinAbs(20.0, 0.1));
        REQUIRE_THAT(range.denormalize(1.f), WithinAbs(20000.0, 1.0));
    }

    SECTION("Logarithmic midpoint is geometric mean") {
        auto range = ParamRange::logarithmic(100.f, 10000.f);

        // Geometric mean of 100 and 10000 is 1000
        float mid = range.denormalize(0.5f);
        REQUIRE_THAT(mid, WithinRel(1000.0, 0.01));
    }

    SECTION("Logarithmic range round-trips correctly") {
        auto range = ParamRange::frequency(20.f, 20000.f);

        for (float v : {20.f, 100.f, 1000.f, 10000.f, 20000.f}) {
            float normalized = range.normalize(v);
            float denormalized = range.denormalize(normalized);
            REQUIRE_THAT(denormalized, WithinRel(double(v), 0.001));
        }
    }
}

// ============================================================================
// MARK: - Snap and Clamp Tests
// ============================================================================

TEST_CASE("ParamRange snap", "[param][range]") {

    SECTION("Snaps to step values") {
        auto range = ParamRange::linear(0.f, 10.f, 1.f);

        REQUIRE_THAT(range.snap(0.4f), WithinAbs(0.0, 0.0001));
        REQUIRE_THAT(range.snap(0.5f), WithinAbs(1.0, 0.0001));
        REQUIRE_THAT(range.snap(0.6f), WithinAbs(1.0, 0.0001));
        REQUIRE_THAT(range.snap(5.3f), WithinAbs(5.0, 0.0001));
        REQUIRE_THAT(range.snap(5.7f), WithinAbs(6.0, 0.0001));
    }

    SECTION("Snap clamps to range") {
        auto range = ParamRange::linear(0.f, 10.f, 1.f);

        REQUIRE_THAT(range.snap(-5.f), WithinAbs(0.0, 0.0001));
        REQUIRE_THAT(range.snap(15.f), WithinAbs(10.0, 0.0001));
    }

    SECTION("No snap when step is 0") {
        auto range = ParamRange::linear(0.f, 10.f, 0.f);

        REQUIRE_THAT(range.snap(5.3f), WithinAbs(5.3, 0.0001));
        REQUIRE_THAT(range.snap(5.7f), WithinAbs(5.7, 0.0001));
    }

    SECTION("Denormalize applies snap") {
        auto range = ParamRange::linear(0.f, 10.f, 1.f);

        // 0.55 normalized would be 5.5, which snaps to 6
        REQUIRE_THAT(range.denormalize(0.55f), WithinAbs(6.0, 0.0001));
    }
}

TEST_CASE("ParamRange clamp", "[param][range]") {

    SECTION("Clamps values to range") {
        auto range = ParamRange::linear(0.f, 100.f);

        REQUIRE_THAT(range.clamp(-10.f), WithinAbs(0.0, 0.0001));
        REQUIRE_THAT(range.clamp(50.f), WithinAbs(50.0, 0.0001));
        REQUIRE_THAT(range.clamp(150.f), WithinAbs(100.0, 0.0001));
    }

    SECTION("Normalize clamps before normalizing") {
        auto range = ParamRange::linear(0.f, 100.f);

        REQUIRE_THAT(range.normalize(-50.f), WithinAbs(0.0, 0.0001));
        REQUIRE_THAT(range.normalize(200.f), WithinAbs(1.0, 0.0001));
    }

    SECTION("Denormalize clamps normalized value") {
        auto range = ParamRange::linear(0.f, 100.f);

        REQUIRE_THAT(range.denormalize(-0.5f), WithinAbs(0.0, 0.0001));
        REQUIRE_THAT(range.denormalize(1.5f), WithinAbs(100.0, 0.0001));
    }
}

// ============================================================================
// MARK: - Special Range Types Tests
// ============================================================================

TEST_CASE("ParamRange special types", "[param][range]") {

    SECTION("Toggle range is 0-1 with step 1") {
        auto range = ParamRange::toggle();

        REQUIRE_THAT(range.denormalize(0.f), WithinAbs(0.0, 0.0001));
        REQUIRE_THAT(range.denormalize(1.f), WithinAbs(1.0, 0.0001));
        REQUIRE_THAT(range.denormalize(0.3f), WithinAbs(0.0, 0.0001));  // Snaps to 0
        REQUIRE_THAT(range.denormalize(0.7f), WithinAbs(1.0, 0.0001));  // Snaps to 1
    }

    SECTION("Choice range covers all choices") {
        auto range = ParamRange::choice(5);  // 0, 1, 2, 3, 4

        REQUIRE_THAT(range.denormalize(0.f), WithinAbs(0.0, 0.0001));
        REQUIRE_THAT(range.denormalize(1.f), WithinAbs(4.0, 0.0001));

        // Middle value should snap to an integer
        float mid = range.denormalize(0.5f);
        REQUIRE_THAT(mid, WithinAbs(std::round(mid), 0.0001));
    }

    SECTION("isDiscrete identifies discrete ranges") {
        auto toggle = ParamRange::toggle();
        auto choice = ParamRange::choice(5);
        auto continuous = ParamRange::linear(0.f, 100.f);
        auto stepped = ParamRange::linear(0.f, 10.f, 1.f);

        REQUIRE(toggle.isDiscrete());
        REQUIRE(choice.isDiscrete());
        REQUIRE_FALSE(continuous.isDiscrete());
        REQUIRE(stepped.isDiscrete());
    }

    SECTION("numSteps returns correct count") {
        auto range = ParamRange::linear(0.f, 10.f, 2.f);
        REQUIRE(range.numSteps() == 6);  // 0, 2, 4, 6, 8, 10

        auto choice = ParamRange::choice(4);
        REQUIRE(choice.numSteps() == 4);
    }
}

// ============================================================================
// MARK: - Edge Cases
// ============================================================================

TEST_CASE("ParamRange edge cases", "[param][range]") {

    SECTION("Zero-width range") {
        auto range = ParamRange::linear(5.f, 5.f);

        // Should handle gracefully (though this is a degenerate case)
        REQUIRE_THAT(range.clamp(5.f), WithinAbs(5.0, 0.0001));
        REQUIRE_THAT(range.clamp(0.f), WithinAbs(5.0, 0.0001));
        REQUIRE_THAT(range.clamp(10.f), WithinAbs(5.0, 0.0001));
    }

    SECTION("Very small step values work correctly") {
        auto range = ParamRange::linear(0.f, 1.f, 0.001f);

        float snapped = range.snap(0.5555f);
        REQUIRE_THAT(snapped, WithinAbs(0.555, 0.0005));  // Rounds to nearest 0.001
    }

    SECTION("length() returns correct value") {
        auto range = ParamRange::linear(-50.f, 50.f);
        REQUIRE_THAT(range.length(), WithinAbs(100.0, 0.0001));
    }
}
