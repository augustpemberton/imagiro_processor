//
// Created by August Pemberton on 18/07/2025.
//

#pragma once
#include "Curve.h"
#include "CurvePoint.h"
#include "Point2D.h"
#include "2d/BezierSegment.h"
#include "choc/containers/choc_Value.h"
#include "juce_core/system/juce_PlatformDefs.h"
#include "../valuedata/Serialize.h"
#include "2d/Path2D.h"
#include "choc/text/choc_JSON.h"

using namespace imagiro;

template<>
struct Serializer<Point2D> {
    static choc::value::Value serialize(const Point2D& point) {
        auto val = choc::value::createObject("Point2D");
        val.addMember("x", choc::value::Value(point.x));
        val.addMember("y", choc::value::Value(point.y));
        return val;
    }

    static Point2D load(const choc::value::ValueView& state) {
        Point2D point;
        try {
            point.x = state["x"].getWithDefault(0.f);
            point.y = state["y"].getWithDefault(0.f);
        } catch (...) {
            jassertfalse;
        }
        return point;
    }
};

template<>
struct Serializer<BezierSegment> {
    static choc::value::Value serialize(const BezierSegment& segment) {
        auto val = choc::value::createObject("BezierSegment");
        val.addMember("start", Serializer<Point2D>::serialize(segment.start));
        val.addMember("control1", Serializer<Point2D>::serialize(segment.control1));
        val.addMember("control2", Serializer<Point2D>::serialize(segment.control2));
        val.addMember("end", Serializer<Point2D>::serialize(segment.end));
        return val;
    }

    static BezierSegment load(const choc::value::ValueView& state) {
        BezierSegment segment{Point2D(), Point2D(), Point2D(), Point2D()};
        try {
            segment.start = Serializer<Point2D>::load(state["start"]);
            segment.control1 = Serializer<Point2D>::load(state["control1"]);
            segment.control2 = Serializer<Point2D>::load(state["control2"]);
            segment.end = Serializer<Point2D>::load(state["end"]);
        } catch (...) {
            jassertfalse;
        }
        return segment;
    }
};

template<>
struct Serializer<CurvePoint> {
    static choc::value::Value serialize(const CurvePoint& point) {
        auto val = choc::value::createObject("CurvePoint");
        val.addMember("position", Serializer<Point2D>::serialize(point.position));
        val.addMember("curve", choc::value::Value(point.curve));
        return val;
    }

    static CurvePoint load(const choc::value::ValueView& state) {
        CurvePoint point;
        try {
            if (state.hasObjectMember("x")) {
                // here for compatibility with old mappings, where x and y are directly in the point
                point.position = Serializer<Point2D>::load(state);
            } else {
                point.position = Serializer<Point2D>::load(state["position"]);
            }
            point.curve = state["curve"].getWithDefault(0.f);
        } catch (...) {
            DBG(choc::json::toString(state));
            jassertfalse;
        }
        return point;
    }
};


static Curve loadLegacyCurve(const choc::value::ValueView& state) {
    Curve c;
    for (auto i=0; i<state["segments"].size(); i++) {
        c.addPoint(CurvePoint{
            state["segments"][i]["start"]["x"].getWithDefault(0.f),
            state["segments"][i]["start"]["y"].getWithDefault(0.f),
            0
        });
        if (i == state["segments"].size() - 1) {
            c.addPoint(CurvePoint{
                state["segments"][i]["end"]["x"].getWithDefault(0.f),
                state["segments"][i]["end"]["y"].getWithDefault(0.f),
                0
            });
        }
    }
    return c;
}

template<>
struct Serializer<Curve> {
    static choc::value::Value serialize(const Curve& curve) {
        auto state = choc::value::createEmptyArray();
        for (const auto& [x, point] : curve.getPoints()) {
            state.addArrayElement(Serializer<CurvePoint>::serialize(point));
        }
        return state;
    }

    static Curve load(const choc::value::ValueView& state) {
        Curve curve;
        try {
            if (state.isObject() && state.hasObjectMember("segments")) {
                DBG("loading legacy curve");
                DBG(choc::json::toString(state));
                curve = loadLegacyCurve(state);
            } else {
                DBG("loading new curve");
                DBG(choc::json::toString(state));
                for (const auto& pointState : state) {
                    const auto point = Serializer<CurvePoint>::load(pointState);
                    curve.addPoint(point);
                }
            }
        } catch (...) {
            jassertfalse;
        }
        return curve;
    }
};

template<>
struct Serializer<Path2D> {
    static choc::value::Value serialize(const Path2D& path) {
        auto state = choc::value::createObject("Path2D");
        auto segmentsArray = choc::value::createEmptyArray();

        for (const auto& segment : path.getSegments()) {
            segmentsArray.addArrayElement(Serializer<BezierSegment>::serialize(segment));
        }

        state.addMember("segments", segmentsArray);
        return state;
    }

    static Path2D load(const choc::value::ValueView& state) {
        Path2D path;
        try {
            for (const auto& segmentState : state["segments"]) {
                path.addSegment(Serializer<BezierSegment>::load(segmentState));
            }
        } catch (...) {
            jassertfalse;
        }
        return path;
    }
};

