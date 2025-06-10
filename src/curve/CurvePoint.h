//
// Created by August Pemberton on 09/06/2025.
//

#pragma once
#include "choc/containers/choc_Value.h"

struct CurvePoint {
    float x;
    float y;
    float controlX;
    float controlY;

    choc::value::Value getState() const {
        auto val = choc::value::createObject("CurvePoint");
        val.addMember("x", choc::value::Value(x));
        val.addMember("y", choc::value::Value(y));
        val.addMember("controlX", choc::value::Value(controlX));
        val.addMember("controlY", choc::value::Value(controlY));
        return val;
    }

    static CurvePoint fromState(const choc::value::ValueView& val) {
        const auto x = val["x"].getWithDefault(0.f);
        const auto y = val["y"].getWithDefault(0.f);
        const auto controlX = val["controlX"].getWithDefault(0.f);
        const auto controlY = val["controlY"].getWithDefault(0.f);
        return {x,y,controlX, controlY};
    }
};

