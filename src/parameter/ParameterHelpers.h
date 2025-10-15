/*
  ==============================================================================

    SliderTextDisplay.h
    Created: 21 Jan 2022 12:54:36pm
    Author:  August Pemberton

  ==============================================================================
*/

#pragma once
#include <juce_core/juce_core.h>

#include <imagiro_util/imagiro_util.h>

#include "DisplayValue.h"

namespace imagiro {
    class Parameter;


    struct DisplayFunctions {

        static DisplayValue midiChannelDisplay(float channel, const Parameter* p);
        static int midiChannelInput(juce::String input, const Parameter* p);

        static DisplayValue degreeDisplay(float deg, const Parameter* p);
        static float degreeInput( juce::String freq, const Parameter* p);

        static DisplayValue timeDisplay(float seconds, const Parameter* p);
        static float timeInput(juce::String time, const Parameter* p);

        static DisplayValue freqDisplay(float db, const Parameter* p);
        static float freqInput(juce::String freq, const Parameter* p);

        static DisplayValue dbDisplay(float db, const Parameter* p);
        static float dbInput(juce::String db, const Parameter* p);
        static DisplayValue gainToDbDisplay(float gain, const Parameter* p);

        static DisplayValue midiNumberDisplay(float note, const Parameter* p);
        static float midiNumberInput(juce::String note, const Parameter* p);

        static DisplayValue centDisplay(float cents, const Parameter* p);
        static float centInput(juce::String input, const Parameter* p);

        static DisplayValue semitoneDisplay(float semitones, const Parameter* p);
        static float semitoneInput(juce::String input, const Parameter* p);

        static DisplayValue percentDisplay(float pct, const Parameter* p);
        static DisplayValue percentDisplayFrac(float pct, const Parameter* p);
        static float percentInput(juce::String t, const Parameter* p);

        static long findGCD(long a, long b);
        static juce::String fractionString(float t);

        static DisplayValue syncDisplay(float val, const Parameter* p);
        static float syncInput(juce::String time, const Parameter* p);

        static DisplayValue noteDisplay(float val, const Parameter* p);
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
