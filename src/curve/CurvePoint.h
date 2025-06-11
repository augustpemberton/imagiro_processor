//
// Created by August Pemberton on 09/06/2025.
//

#pragma once
#include "choc/containers/choc_Value.h"

struct CurvePoint {
    float x;
    float y;
    float curve;

    choc::value::Value getState() const {
        auto val = choc::value::createObject("CurvePoint");
        val.addMember("x", choc::value::Value(x));
        val.addMember("y", choc::value::Value(y));
        val.addMember("curve", choc::value::Value(curve));
        return val;
    }

    static CurvePoint fromState(const choc::value::ValueView& val) {
        const auto x = val["x"].getWithDefault(0.f);
        const auto y = val["y"].getWithDefault(0.f);
        const auto curve = val["curve"].getWithDefault(0.f);
        return {x, y, curve};
    }
};