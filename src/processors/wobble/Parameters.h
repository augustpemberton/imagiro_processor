#pragma once

namespace WobbleProcessorParameters {
    static constexpr const char* PARAMETERS_YAML = R"(

rate:
    name: "rate"
    default: 0.4
    type: percent
    range: { min: 0, max: 1}

damp:
    name: "damp"
    default: 0.2
    range: {min: 0, max: 1}
    type: percent

depth:
    name: "depth"
    default: 0.3
    range: {min: 0, max: 1}
    type: percent

mix:
    name: "mix"
    type: percent
    default: 1

    )";

};