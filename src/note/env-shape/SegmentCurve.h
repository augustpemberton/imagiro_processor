//
// Created by August Pemberton on 30/08/2023.
//

#pragma once
#include "Segment.h"

class SegmentCurve {
public:
    SegmentCurve();
    SegmentCurve(std::set<Segment> segments);

    float getY(float x) const;

    choc::value::Value getState() const;
    static SegmentCurve fromState(const choc::value::ValueView& state);
private:
    std::set<Segment> segments;
};
