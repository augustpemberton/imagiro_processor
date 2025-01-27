/*
  ==============================================================================

    SliderTextDisplay.h
    Created: 21 Jan 2022 12:54:36pm
    Author:  August Pemberton

  ==============================================================================
*/

#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include <imagiro_util/imagiro_util.h>

namespace imagiro {

    struct DisplayValue {
        juce::String value;
        juce::String suffix {""};

        juce::String withSuffix() { return value + suffix; }
        juce::String withoutSuffix() { return value; }
    };

    struct DisplayFunctions {

        static DisplayValue midiChannelDisplay(float channel);
        static int midiChannelInput(juce::String input);

        static DisplayValue degreeDisplay(float deg);
        static float degreeInput( juce::String freq);

        static DisplayValue timeDisplay(float seconds);
        static float timeInput(juce::String time);

        static DisplayValue freqDisplay(float db);
        static float freqInput(juce::String freq);

        static DisplayValue dbDisplay(float db);
        static float dbInput(juce::String db);
        static DisplayValue gainToDbDisplay(float gain);

        static DisplayValue midiNumberDisplay(float note);
        static float midiNumberInput(juce::String note);

        static DisplayValue centDisplay(float cents);
        static float centInput(juce::String input);

        static DisplayValue semitoneDisplay(float semitones);
        static float semitoneInput(juce::String input);

        static DisplayValue percentDisplay(float pct);
        static DisplayValue percentDisplayFrac(float pct);
        static float percentInput(juce::String t);

        static long findGCD(long a, long b);
        static juce::String fractionString(float t);

        static DisplayValue syncDisplay(float val);
        static float syncInput(juce::String time);

        static DisplayValue noteDisplay(float val);
    };

    static juce::NormalisableRange<float> getTempoSyncRange(int minPower, int maxPower, bool inverse) {
        auto range = juce::NormalisableRange<float>{
                powf(2, minPower),
                powf(2, maxPower),
                [=](auto rangeStart, auto rangeEnd, auto valueToRemap) {  // to real world from 01
                    if (inverse) valueToRemap = 1 - valueToRemap;
                    auto power = juce::jmap(valueToRemap, (float) minPower, (float) maxPower);
                    auto s = pow(2, power);
                    return juce::jlimit((float)rangeStart, (float)rangeEnd, (float)s);
                },
                [=](auto rangeStart, auto rangeEnd, auto valueToRemap)  // to 01 from real world
                {
                    valueToRemap = juce::jlimit((float)rangeStart, (float)rangeEnd,
                                                (float)valueToRemap);
                    auto power = prevPowerOfTwo(valueToRemap);
                    power = juce::jlimit(minPower, maxPower, power);
                    auto s = juce::jmap((float) power, (float) minPower, (float) maxPower, 0.0f, 1.0f);
                    return inverse ? 1 - s : s;
                },
                [](auto rangeStart, auto rangeEnd, auto valueToRemap) { // real world to valid real world
                    auto s = pow(2, prevPowerOfTwo(valueToRemap));
                    return juce::jlimit((float)rangeStart, (float)rangeEnd, (float)s);
                }
        };

        return range;
    }
}