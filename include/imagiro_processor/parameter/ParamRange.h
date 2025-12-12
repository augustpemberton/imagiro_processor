#pragma once

#include <cmath>
#include <algorithm>
#include <variant>

namespace imagiro {
    class ParamRange {
    public:
        struct Linear { float skew{1.f}; };
        struct Logarithmic { };

        using Mapping = std::variant<Linear, Logarithmic>;

        float min_ {0.f}, max_ {1.f}, step_{0.f};
        Mapping mapping_{Linear{}};

        constexpr ParamRange() = default;

        static constexpr ParamRange linear(float min, float max, float step = 0.f) {
            return {min, max, step, Linear{1.f}};
        }

        static constexpr ParamRange skewed(float min, float max, float skew, float step = 0.f) {
            return {min, max, step, Linear{skew}};
        }

        static constexpr ParamRange toggle() {
            return {0.f, 1.f, 1.f, Linear{1.f}};
        }

        static constexpr ParamRange choice(int numChoices) {
            return {0.f, static_cast<float>(numChoices - 1), 1.f, Linear{1.f}};
        }

        static constexpr ParamRange frequency(float minHz = 20.f, float maxHz = 20000.f) {
            return {minHz, maxHz, 0.f, Logarithmic{}};
        }

        static constexpr ParamRange logarithmic(float min, float max, float step = 0.f) {
            return {min, max, step, Logarithmic{}};
        }

        float normalize(float value) const {
            const float clamped = std::clamp(value, min_, max_);

            return std::visit([&]<typename T0>(const T0 &m) -> float {
                using T = std::decay_t<T0>;

                if constexpr (std::is_same_v<T, Logarithmic>) {
                    return std::log(clamped / min_) / std::log(max_ / min_);
                }
                float normalized = (clamped - min_) / (max_ - min_);
                if constexpr (std::is_same_v<T, Linear>) {
                    if (m.skew != 1.f) {
                        normalized = std::pow(normalized, 1.f / m.skew);
                    }
                }
                return normalized;
            }, mapping_);
        }

        float denormalize(float normalized) const {
            const float clamped = std::clamp(normalized, 0.f, 1.f);

            const float value = std::visit([&]<typename T0>(const T0 &m) -> float {
                using T = std::decay_t<T0>;

                if constexpr (std::is_same_v<T, Logarithmic>) {
                    return min_ * std::pow(max_ / min_, clamped);
                }

                float shaped = clamped;

                if constexpr (std::is_same_v<T, Linear>) {
                    if (m.skew != 1.f) {
                        shaped = std::pow(clamped, m.skew);
                    }
                }

                return min_ + shaped * (max_ - min_);
            }, mapping_);

            return snap(value);
        }

        float snap(float value) const {
            if (step_ > 0.f) {
                value = std::round((value - min_) / step_) * step_ + min_;
            }
            return std::clamp(value, min_, max_);
        }

        float clamp(float value) const {
            return std::clamp(value, min_, max_);
        }

        constexpr float length() const {
            return max_ - min_;
        }

        int numSteps() const {
            if (step_ <= 0.f) return 0;
            return static_cast<int>(length() / step_) + 1;
        }

        bool isDiscrete() const {
            return step_ > 0.f && numSteps() <= 100;
        }

    private:
        constexpr ParamRange(float min, float max, float step, Mapping mapping)
            : min_(min), max_(max), step_(step), mapping_(mapping) {
        }
    };
}