//
// Created by August Pemberton on 18/07/2025.
//

#pragma once
#include "Curve.h"
#include "CurvePoint.h"
#include "Point2D.h"
#include "2d/BezierSegment.h"
#include <nlohmann/json.hpp>
#include "juce_core/system/juce_PlatformDefs.h"
#include "../valuedata/Serialize.h"
#include "2d/Path2D.h"

using namespace imagiro;
using json = nlohmann::json;

template<>
struct Serializer<Point2D> {
    static json serialize(const Point2D& point) {
        auto val = json::object();
        val["x"] = point.x;
        val["y"] = point.y;
        return val;
    }

    static Point2D load(const json& state) {
        Point2D point;
        try {
            point.x = state.value("x", 0.f);
            point.y = state.value("y", 0.f);
        } catch (...) {
            jassertfalse;
        }
        return point;
    }
};

template<>
struct Serializer<BezierSegment> {
    static json serialize(const BezierSegment& segment) {
        auto val = json::object();
        val["start"] = Serializer<Point2D>::serialize(segment.start);
        val["control1"] = Serializer<Point2D>::serialize(segment.control1);
        val["control2"] = Serializer<Point2D>::serialize(segment.control2);
        val["end"] = Serializer<Point2D>::serialize(segment.end);
        return val;
    }

    static BezierSegment load(const json& state) {
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
    static json serialize(const CurvePoint& point) {
        auto val = json::object();
        val["position"] = Serializer<Point2D>::serialize(point.position);
        val["curve"] = point.curve;
        return val;
    }

    static CurvePoint load(const json& state) {
        CurvePoint point;
        try {
            if (state.contains("x")) {
                // here for compatibility with old mappings, where x and y are directly in the point
                point.position = Serializer<Point2D>::load(state);
            } else {
                point.position = Serializer<Point2D>::load(state["position"]);
            }
            point.curve = state.value("curve", 0.f);
        } catch (...) {
            DBG(state.dump());
            jassertfalse;
        }
        return point;
    }
};


static Curve loadLegacyCurve(const json& state) {
    Curve c;
    auto& segments = state["segments"];
    for (size_t i = 0; i < segments.size(); i++) {
        c.addPoint(CurvePoint{
            segments[i]["start"].value("x", 0.f),
            segments[i]["start"].value("y", 0.f),
            0
        });
        if (i == segments.size() - 1) {
            c.addPoint(CurvePoint{
                segments[i]["end"].value("x", 0.f),
                segments[i]["end"].value("y", 0.f),
                0
            });
        }
    }
    return c;
}

template<>
struct Serializer<Curve> {
    static json serialize(const Curve& curve) {
        auto state = json::array();
        for (const auto& [x, point] : curve.getPoints()) {
            state.push_back(Serializer<CurvePoint>::serialize(point));
        }
        return state;
    }

    static Curve load(const json& state) {
        Curve curve;
        try {
            if (state.is_object() && state.contains("segments")) {
                curve = loadLegacyCurve(state);
            } else {
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
    static json serialize(const Path2D& path) {
        auto state = json::object();
        auto segmentsArray = json::array();

        for (const auto& segment : path.getSegments()) {
            segmentsArray.push_back(Serializer<BezierSegment>::serialize(segment));
        }

        state["segments"] = segmentsArray;
        return state;
    }

    static Path2D load(const json& state) {
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
