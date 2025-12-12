//
// Created by August Pemberton on 11/12/2025.
//

#pragma once


#pragma once

#include <string>
#include <functional>
#include <optional>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace imagiro {
    struct ParamFormat {
        std::function<std::string(float userValue)> toString;
        std::function<std::optional<float>(const std::string &text)> fromString;

        // Default: just show the number
        static ParamFormat number(int decimalPlaces = 2) {
            return {
                [=](float v) {
                    std::ostringstream ss;
                    ss << std::fixed << std::setprecision(decimalPlaces) << v;
                    return ss.str();
                },
                [](const std::string &s) -> std::optional<float> {
                    try { return std::stof(s); } catch (...) { return std::nullopt; }
                }
            };
        }

        static ParamFormat withSuffix(const std::string& suffix, int decimalPlaces = 2) {
            return {
                [=](float v) {
                    std::ostringstream ss;
                    ss << std::fixed << std::setprecision(decimalPlaces) << v;
                    if (!suffix.empty()) ss << " " << suffix;
                    return ss.str();
                },
                [](const std::string& s) -> std::optional<float> {
                    try { return std::stof(s); }
                    catch (...) { return std::nullopt; }
                }
            };
        }

        static ParamFormat decibels(int decimalPlaces = 1) {
            return {
                [=](float v) {
                    std::ostringstream ss;
                    ss << std::fixed << std::setprecision(decimalPlaces) << v << " dB";
                    return ss.str();
                },
                [](const std::string &s) -> std::optional<float> {
                    try {
                        // Strip "dB" suffix if present
                        std::string clean = s;
                        auto pos = clean.find("dB");
                        if (pos != std::string::npos) clean = clean.substr(0, pos);
                        return std::stof(clean);
                    } catch (...) { return std::nullopt; }
                }
            };
        }

        static ParamFormat frequency(int decimalPlaces = 0) {
            return {
                [=](float hz) {
                    std::ostringstream ss;
                    if (hz >= 1000.f) {
                        ss << std::fixed << std::setprecision(decimalPlaces + 1) << (hz / 1000.f) << " kHz";
                    } else {
                        ss << std::fixed << std::setprecision(decimalPlaces) << hz << " Hz";
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

                        // Extract just the number
                        return std::stof(clean) * multiplier;
                    } catch (...) { return std::nullopt; }
                }
            };
        }

        static ParamFormat percent(int decimalPlaces = 0) {
            return {
                [=](float v) {
                    std::ostringstream ss;
                    ss << std::fixed << std::setprecision(decimalPlaces) << (v * 100.f) << "%";
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

        static ParamFormat time(int decimalPlaces = 2) {
            return {
                [=](float seconds) {
                    std::ostringstream ss;
                    if (seconds >= 1.f) {
                        ss << std::fixed << std::setprecision(decimalPlaces) << seconds << " s";
                    } else {
                        ss << std::fixed << std::setprecision(std::max(0, decimalPlaces - 1)) << (seconds * 1000.f) <<
                                " ms";
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

        static ParamFormat toggle() {
            return {
                [](float v) { return v > 0.5f ? "On" : "Off"; },
                [](const std::string &s) -> std::optional<float> {
                    if (s == "On" || s == "on" || s == "1" || s == "true") return 1.f;
                    if (s == "Off" || s == "off" || s == "0" || s == "false") return 0.f;
                    return std::nullopt;
                }
            };
        }

        static ParamFormat choice(const std::vector<std::string> &choices) {
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