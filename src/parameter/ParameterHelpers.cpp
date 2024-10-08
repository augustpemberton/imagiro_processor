//
// Created by August Pemberton on 12/09/2022.
//

#pragma once
#include "ParameterHelpers.h"
#include <imagiro_util/imagiro_util.h>

namespace imagiro {

    DisplayValue DisplayFunctions::midiChannelDisplay(float channel) {
        auto c = (int) channel;
        if (c == 0) return {"all"};
        else return {juce::String(c)};
    }

    int DisplayFunctions::midiChannelInput(juce::String input) {
        if (input.toLowerCase() == "all") return 0;
        return input.getIntValue();
    }

    DisplayValue DisplayFunctions::timeDisplay(const Parameter& p, float seconds) {
        if (seconds == 0) return {"0", "ms"};
        std::stringstream ss;
        juce::String suf;
        if (seconds >= 0.999) {
            ss << std::fixed << std::setprecision(2) << seconds;
            suf = "s";
        } else if (seconds >= 0.0001) {
            auto nDigits = floor(log10(seconds * 1000) + 1);
            ss << std::fixed << std::setprecision(3 - nDigits) << seconds * 1000;
            suf = "ms";
        } else {
            ss << std::fixed << std::setprecision(3) << seconds * 1000;
            suf = "ms";
        }
        return {ss.str(), suf};
    }

    float DisplayFunctions::timeInput(const Parameter& p, juce::String time) {
        float multiplier = 1;
        if (time.contains("ms")) {
            multiplier = 0.001f;
            time = time.replace("ms", "", true);
        } else if (time.contains("s")) {
            multiplier = 1;
            time = time.replace("s", "", true);
        }

        return time.getFloatValue() * multiplier;

    }

    DisplayValue DisplayFunctions::dbDisplay(const Parameter& p, float db) {
        if (db <= -60) return {"-inf", ""};
        if (std::abs(db) < 0.0001) return {"0.00", "db"};

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << db;
        return {juce::String(oss.str()), "db"};
    }

    DisplayValue DisplayFunctions::freqDisplay(const Parameter& p, float freq) {
        std::ostringstream oss;
        if (freq >= 10000) {
            oss << std::fixed << std::setprecision(1) << freq / 1000 << "k";
        } else if (freq > 100){
            oss << std::fixed << (int) freq;
        } else if (freq > 10){
            oss << std::fixed << std::setprecision(1) << freq;
        } else {
            oss << std::fixed << std::setprecision(2) << freq;
        }

        return {oss.str(), "Hz"};
    }

    float DisplayFunctions::freqInput(const Parameter& p, juce::String freq) {
        freq = freq.replace("Hz", "", true);
        return freq.getFloatValue();
    }

    float DisplayFunctions::dbInput(const Parameter& p, juce::String db) {
        db = db.replace("db", "", true);
        return db.getFloatValue();
    }

    DisplayValue DisplayFunctions::gainToDbDisplay(const Parameter& p, float gain) {
        auto db = juce::Decibels::gainToDecibels(gain);
        return dbDisplay(p, db);
    }

    DisplayValue DisplayFunctions::midiNumberDisplay(const Parameter& p, float note) {
        return {juce::String(Note::midiNumberToNote(note))};
    }

    float DisplayFunctions::midiNumberInput(juce::String note) {
        return Note::noteToMidiNumber(note);
    }

    DisplayValue DisplayFunctions::centDisplay(const Parameter& p, float semitones) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << semitones;
        return {ss.str(), "st"};
    }

    float DisplayFunctions::centInput(const Parameter& p, juce::String input) {
        input = input.replace("st", "\n", true);
        input = input.replace("ct", "", true);
        auto tokens = juce::StringArray::fromLines(input);

        if (tokens.size() == 1)
            return tokens[0].getFloatValue();
        else if (tokens.size() == 2)
            return tokens[0].getFloatValue() * 100 + tokens[1].getIntValue();

        return 0;
    }

    DisplayValue DisplayFunctions::semitoneDisplay(const Parameter& p, float st) {
        int semis = static_cast<int>(round(st));
        int cents = static_cast<int>((st - semis) * 100);

        std::string centString = (cents >= 0 ? "+" : "-") + std::to_string(abs(cents)) + "ct";
        return {juce::String(semis) + "st " + centString};
    }

    float DisplayFunctions::semitoneInput(const Parameter& p, juce::String input) {
        return centInput(p, input);
    }

    DisplayValue DisplayFunctions::percentDisplay(const Parameter& p, float pct) {
        return {juce::String(round(pct * 100)), "%"};
    }

    DisplayValue DisplayFunctions::percentDisplayFrac(const Parameter& p, float pct) {
        return {juce::String(pct * 100, 1), "%"};
    }

    float DisplayFunctions::percentInput(const Parameter& p, juce::String t) {
        t = t.replace("%", "");
        return t.getFloatValue() / 100;
    }

    long DisplayFunctions::findGCD(long a, long b) {
        if (a == 0)
            return b;
        else if (b == 0)
            return a;

        if (a < b)
            return findGCD(a, b % a);
        else
            return findGCD(b, a % b);
    }

    juce::String DisplayFunctions::fractionString(float t) {
        juce::String s;

        // snap to power of 2
        t = pow(2, prevPowerOfTwo(t));

        double integral = std::floor(t);
        double frac = t - integral;

        const long precision = 64; // This is the accuracy.

        long gcd = findGCD(round(frac * precision), precision);

        float n2 = precision / gcd;
        float n1 = round(frac * precision) / gcd + n2 * integral;

        if (almostEqual(n1, trunc(n1)) && almostEqual(n2, trunc(n2)))
            s = n2 > 1 ? juce::String(n1) + "/" + juce::String(n2)
                       : juce::String(n1);
        else
            s = juce::String(t);

        return s;
    }

    DisplayValue DisplayFunctions::degreeDisplay(const Parameter &p, float deg) {
        return {juce::String(deg), "°"};
    }

    float DisplayFunctions::degreeInput(const Parameter& p, juce::String deg) {
        deg = deg.replace("°", "", true);
        return deg.getFloatValue();
    }

    DisplayValue DisplayFunctions::syncDisplay(const Parameter &p, float val) {
        auto f = fractionString(val);
        return {f};
    }
    float DisplayFunctions::syncInput(const Parameter& p, juce::String frac) {
        if (!frac.contains("/")) return frac.getFloatValue();

        juce::StringArray f;
        f.addTokens(frac, "/", "");
        if (f.size() != 2) return f[0].getFloatValue();
        return f[0].getFloatValue() / f[1].getFloatValue();
    }
}
