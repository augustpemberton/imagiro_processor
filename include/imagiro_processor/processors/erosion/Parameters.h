#pragma once

namespace ErosionProcessorParameters {
    static constexpr const char* PARAMETERS_YAML = R"(


bypass:
    name: "bypass"
    default: 0
    type: toggle

gain:
  name: "gain"
  type: db
  default: -30
  range: {min: -60, max: 12, step: 0.1}

lowpass:
    name: "lp"
    type: freq
    default: 2000
    range: {min: 20, max: 20000, type: freq}

highpass:
    name: "hp"
    type: freq
    default: 300
    range: {min: 20, max: 20000, type: freq}

)";

};