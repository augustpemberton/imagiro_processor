//
// Created by August Pemberton on 12/09/2022.
//

#pragma once
#include "ParameterHelpers.h"
#include <imagiro_util/imagiro_util.h>

namespace imagiro {

    DisplayValue DisplayFunctions::midiChannelDisplay(float channel, const Parameter*) {
        auto c = (int) channel;
        if (c == 0) return {"all"};
        else return {juce::String(c)};
    }

    int DisplayFunctions::midiChannelInput(juce::String input, const Parameter*) {
        if (input.toLowerCase() == "all") return 0;
        return input.getIntValue();
    }

    DisplayValue DisplayFunctions::timeDisplay(float seconds, const Parameter*) {
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

    float DisplayFunctions::timeInput(juce::String time, const Parameter*) {
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

    DisplayValue DisplayFunctions::dbDisplay(float db, const Parameter*) {
        if (db <= -60) return {"-inf", ""};
        if (std::abs(db) < 0.0001) return {"0.00", "db"};

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << db;
        return {juce::String(oss.str()), "db"};
    }

    DisplayValue DisplayFunctions::freqDisplay(float freq, const Parameter*) {
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

    float DisplayFunctions::freqInput(juce::String freq, const Parameter*) {
        freq = freq.replace("Hz", "", true);
        return freq.getFloatValue();
    }

    float DisplayFunctions::dbInput(juce::String db, const Parameter*) {
        db = db.replace("db", "", true);
        return db.getFloatValue();
    }

    DisplayValue DisplayFunctions::gainToDbDisplay(float gain, const Parameter* p) {
        auto db = juce::Decibels::gainToDecibels(gain);
        return dbDisplay(db, p);
    }

    DisplayValue DisplayFunctions::midiNumberDisplay(float note, const Parameter*) {
        return {juce::String(Note::midiNumberToNote(note))};
    }

    float DisplayFunctions::midiNumberInput(juce::String note, const Parameter*) {
        return Note::noteToMidiNumber(note);
    }

    DisplayValue DisplayFunctions::centDisplay(float semitones, const Parameter*) {
        if (almostEqual(semitones, 0.f)) return {"0.00", "st"}; // prevent -0.00
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << semitones;
        return {ss.str(), "st"};
    }

    float DisplayFunctions::centInput(juce::String input, const Parameter*) {
        input = input.replace("st", "\n", true);
        input = input.replace("ct", "", true);
        auto tokens = juce::StringArray::fromLines(input);

        if (tokens.size() == 1)
            return tokens[0].getFloatValue();
        else if (tokens.size() == 2)
            return tokens[0].getFloatValue() * 100 + tokens[1].getIntValue();

        return 0;
    }

    DisplayValue DisplayFunctions::semitoneDisplay(float st, const Parameter*) {
        int semis = static_cast<int>(round(st));
        int cents = static_cast<int>((st - semis) * 100);

        std::string centString = (cents >= 0 ? "+" : "-") + std::to_string(abs(cents)) + "ct";
        return {juce::String(semis) + "st " + centString};
    }

    float DisplayFunctions::semitoneInput(juce::String input, const Parameter* p) {
        return centInput(input, p);
    }

    DisplayValue DisplayFunctions::percentDisplay(float pct, const Parameter* p) {
        return {juce::String(round(pct * 100)), "%"};
    }

    DisplayValue DisplayFunctions::percentDisplayFrac(float pct, const Parameter*) {
        return {juce::String(pct * 100, 1), "%"};
    }

    float DisplayFunctions::percentInput(juce::String t, const Parameter*) {
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

    DisplayValue DisplayFunctions::degreeDisplay(float v, const Parameter*) {
        return {juce::String((int)(v*360)), juce::CharPointer_UTF8("\u00B0")};
    }

    float DisplayFunctions::degreeInput(juce::String deg, const Parameter*) {
        deg = deg.replace("°", "", true);
        return deg.getFloatValue() / 360.f;
    }

    DisplayValue DisplayFunctions::syncDisplay(float val, const Parameter*) {
        auto f = fractionString(val);
        return {f, " note"};
    }
    float DisplayFunctions::syncInput(juce::String time, const Parameter*) {
        time = time.trim();
        if (!time.contains("/")) return time.getFloatValue();

        juce::StringArray f;
        f.addTokens(time, "/", "");
        if (f.size() != 2) return f[0].getFloatValue();
        return f[0].getFloatValue() / f[1].getFloatValue();
    }
}
