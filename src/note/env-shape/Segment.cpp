//
// Created by August Pemberton on 30/08/2023.
//
#include "Segment.h"

Segment::Segment(CurvePoint s, CurvePoint e, CurvePoint c)
        : start(s), end(e), control(c)
{
    reloadTable();
}

Segment::Segment(const Segment &other)
        : start(other.start),
          end(other.end),
          control(other.control)
{
    reloadTable();
}

Segment& Segment::operator=(const Segment& other) {
    start = other.start;
    end = other.end;
    control = other.control;
    reloadTable();
    return *this;
}

float Segment::getY(float x) const {
    return lookupTable->getUnchecked(x * (lookupTable->getNumPoints() - 1));
}

std::unique_ptr<juce::dsp::LookupTable<float>>
Segment::createBezierCurve(CurvePoint start, CurvePoint end, CurvePoint control, int numCurvePoints) {
    Bezier bezier (start, end, control);
    return std::make_unique<juce::dsp::LookupTable<float>>([&](float p) -> float {
        auto x = p / (float) numCurvePoints;
        return bezier.getY(x);
    }, numCurvePoints);
}

void Segment::reloadTable() {
    lookupTable = createBezierCurve(start, end, control);
}

//============ state

static choc::value::Value getCurvePointState(CurvePoint point) {
    auto state = choc::value::createObject("CurvePoint");
    state.addMember("x", point.x);
    state.addMember("y", point.y);
    return state;
}

static CurvePoint pointFromState(choc::value::ValueView& state) {
    return {
            std::min(1.f, std::max(0.f, state["x"].getWithDefault(0.f))),
            std::min(1.f, std::max(0.f, state["y"].getWithDefault(0.f)))
    };
}

choc::value::Value Segment::getState() const {
    auto state = choc::value::createObject("Segment");
    state.addMember("start", getCurvePointState(start));
    state.addMember("end", getCurvePointState(end));
    state.addMember("control", getCurvePointState(control));
    return state;
}

Segment Segment::fromState(choc::value::ValueView &state) {
    auto startState = state["start"];
    auto start = pointFromState(startState);
    auto endState = state["end"];
    auto end = pointFromState(endState);
    auto controlState = state["control"];
    auto control = pointFromState(controlState);

    return {start, end, control};
}




