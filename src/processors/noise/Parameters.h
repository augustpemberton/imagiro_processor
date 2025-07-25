#pragma once

namespace NoiseProcessorParameters {
static constexpr const char* PARAMETERS_YAML = R"(


bypass:
    name: "bypass"
    default: 0
    type: toggle

type:
    name: "type"
    type: choice
    choices: ['white', 'file']

tightness:
    name: "tightness"
    type: time
    default: 0.05
    range: {min: 0.005, max: 1, skew: 0.6}

gain:
  name: "gain"
  type: db
  default: -12
  range: {min: -60, max: 24, step: 0.1}

lowpass:
    name: "lp"
    type: freq
    default: 20000
    range: {min: 20, max: 20000, type: freq}

highpass:
    name: "hp"
    type: freq
    default: 300
    range: {min: 20, max: 20000, type: freq}

pitch:
    name: "pitch"
    type: cent
    default: 0
    range: {min: -24, max: 24, step: 0.01}

)";

};
