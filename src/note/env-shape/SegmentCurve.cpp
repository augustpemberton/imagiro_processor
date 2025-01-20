//
// Created by August Pemberton on 30/08/2023.
//

#include "SegmentCurve.h"

SegmentCurve::SegmentCurve() {
    auto s = std::set<Segment>();
    s.insert(Segment({0.f, 0.f},
                     {1.f, 1.f},
                     {0.5f, 0.5f}));
    segments = s;
}

SegmentCurve::SegmentCurve(std::set<Segment> s) {
    segments = s;
}

float SegmentCurve::getY(float x) const {
    // segments is a sorted set, so just loop until we find the first suitable segment
    for (auto &segment: segments) {
        if (segment.getStart().x < x && segment.getEnd().x >= x) {
            auto y = segment.getY(x);
            return juce::jlimit(0.f, 1.f, y);
        }
    }

    return 1.f;
}

choc::value::Value SegmentCurve::getState() const {
    auto state = choc::value::createObject("SegmentCurve");
    auto segmentsState = choc::value::createEmptyArray();
    for (auto &segment: segments) {
        segmentsState.addArrayElement(segment.getState());
    }
    state.addMember("segments", segmentsState);
    return state;
}

SegmentCurve SegmentCurve::fromState(const choc::value::ValueView &state) {
    std::set<Segment> newSegments;
    for (auto segmentState: state["segments"]) {
        newSegments.insert(Segment::fromState(segmentState));
    }
    return SegmentCurve(newSegments);
}

