//
// Created by August Pemberton on 30/08/2023.
//

#pragma once
#include "Bezier.h"

#define DEFAULT_TABLE_SIZE 64
class Segment {
public:
    Segment(CurvePoint start, CurvePoint end, CurvePoint control);
    Segment(const Segment& other);

    Segment& operator=(const Segment& other);

    bool operator <(const Segment& rhs) const
    {
        return start.x < rhs.start.x;
    }

    float getY(float x) const;
    CurvePoint getStart() const { return start; }
    CurvePoint getEnd() const { return end; }
    CurvePoint getControl() const { return control; }

    choc::value::Value getState() const;
    static Segment fromState(choc::value::ValueView& state);
private:
    void reloadTable();
    std::unique_ptr<juce::dsp::LookupTable<float>> lookupTable;
    CurvePoint start;
    CurvePoint end;
    CurvePoint control;

    static std::unique_ptr<juce::dsp::LookupTable<float>> createBezierCurve(
            CurvePoint start, CurvePoint end,
            CurvePoint control, int numCurvePoints = DEFAULT_TABLE_SIZE);
};
