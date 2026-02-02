#pragma once

#include <string>
#include <functional>
#include <optional>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace imagiro {
    struct ValueFormatter {
        std::function<std::string(float userValue)> toString;
        std::function<std::optional<float>(const std::string &text)> fromString;

        static float sanitize(float v) {
            return std::abs(v) < 1e-5f ? 0.f : v;
        }

        static int decimalPlacesForDigits(float v, int totalDigits) {
            if (v == 0.f) return totalDigits - 1;
            int intDigits = static_cast<int>(std::floor(std::log10(std::abs(v)))) + 1;
            return std::max(0, totalDigits - intDigits);
        }

        static ValueFormatter number(int digits = 3) {
            return withSuffix("", digits);
        }

        static ValueFormatter range(ParamRange range, int digits = 3) {
            const auto fmt = number(digits);
            return {
                [fmt, range](float v) { return fmt.toString(range.denormalize(v)); },
                [fmt, range](std::string v) { return range.normalize(fmt.fromString(v).value_or(0)); },
            };
        }

        static ValueFormatter withSuffix(const std::string& suffix, int digits = 3) {
            return {
                [=](float v) {
                    v = sanitize(v);
                    std::ostringstream ss;
                    ss << std::fixed << std::setprecision(decimalPlacesForDigits(v, digits)) << v;
                    if (!suffix.empty()) ss << suffix;
                    return ss.str();
                },
                [](const std::string& s) -> std::optional<float> {
                    try { return std::stof(s); }
                    catch (...) { return std::nullopt; }
                }
            };
        }

        static ValueFormatter decibels(int digits = 3) {
            return withSuffix("dB", digits);
        }

        static ValueFormatter frequency(int digits = 3) {
            return {
                [=](float hz) {
                    hz = sanitize(hz);
                    std::ostringstream ss;
                    if (hz >= 1000.f) {
                        float khz = hz / 1000.f;
                        ss << std::fixed << std::setprecision(decimalPlacesForDigits(khz, digits)) << khz << " kHz";
                    } else {
                        ss << std::fixed << std::setprecision(decimalPlacesForDigits(hz, digits)) << hz << " Hz";
                    }
                    return ss.str();
                },
                [](const std::string &s) -> std::optional<float> {
                    try {
                        std::string clean = s;
                        float multiplier = 1.f;

                        if (clean.find("kHz") != std::string::npos ||
                            clean.find("khz") != std::string::npos ||
                            clean.find("k") != std::string::npos) {
                            multiplier = 1000.f;
                        }

                        return std::stof(clean) * multiplier;
                    } catch (...) { return std::nullopt; }
                }
            };
        }

        static ValueFormatter percent(int digits = 3) {
            return {
                [=](float v) {
                    float pct = sanitize(v * 100.f);
                    std::ostringstream ss;
                    ss << std::fixed << std::setprecision(decimalPlacesForDigits(pct, digits)) << pct << "%";
                    return ss.str();
                },
                [](const std::string &s) -> std::optional<float> {
                    try {
                        std::string clean = s;
                        auto pos = clean.find('%');
                        if (pos != std::string::npos) clean = clean.substr(0, pos);
                        return std::stof(clean) / 100.f;
                    } catch (...) { return std::nullopt; }
                }
            };
        }

        static ValueFormatter pan() {
            return {
                [=](float v) {
                    const int mag = static_cast<int>(std::round(std::abs(v-0.5f) * 100));
                    const auto dir =  almostEqual(v, 0.5f) ? "C" : v > 0.5 ? "R" : "L";
                    std::ostringstream ss;
                    ss << std::fixed << mag << dir;
                    return ss.str();
                },
                [](const std::string &s) -> std::optional<float> {
                    try {
                        const std::string clean = s.substr(0, s.size() - 1);
                        const auto mag = std::stof(clean) * 0.01f;
                        return 0.5 + (s.back() == 'R' ? mag : -mag);
                    } catch (...) { return std::nullopt; }
                }
            };
        }

        static ValueFormatter time(int digits = 3) {
            return {
                [=](float seconds) {
                    seconds = sanitize(seconds);
                    std::ostringstream ss;
                    if (seconds >= 1.f) {
                        ss << std::fixed << std::setprecision(decimalPlacesForDigits(seconds, digits)) << seconds << " s";
                    } else {
                        float ms = seconds * 1000.f;
                        ss << std::fixed << std::setprecision(decimalPlacesForDigits(ms, digits)) << ms << " ms";
                    }
                    return ss.str();
                },
                [](const std::string &s) -> std::optional<float> {
                    try {
                        std::string clean = s;
                        float multiplier = 1.f;

                        if (clean.find("ms") != std::string::npos) {
                            multiplier = 0.001f;
                        }

                        return std::stof(clean) * multiplier;
                    } catch (...) { return std::nullopt; }
                }
            };
        }

        static ValueFormatter toggle() {
            return {
                [](float v) { return v > 0.5f ? "On" : "Off"; },
                [](const std::string &s) -> std::optional<float> {
                    if (s == "On" || s == "on" || s == "1" || s == "true") return 1.f;
                    if (s == "Off" || s == "off" || s == "0" || s == "false") return 0.f;
                    return std::nullopt;
                }
            };
        }

        static ValueFormatter choice(const std::vector<std::string> &choices) {
            return {
                [choices](float v) {
                    const int idx = static_cast<int>(std::round(v));
                    if (idx >= 0 && idx < static_cast<int>(choices.size())) {
                        return choices[static_cast<size_t>(idx)];
                    }
                    return std::string("?");
                },
                [choices](const std::string &s) -> std::optional<float> {
                    for (size_t i = 0; i < choices.size(); i++) {
                        if (choices[i] == s) return static_cast<float>(i);
                    }
                    return std::nullopt;
                }
            };
        }
    };
}