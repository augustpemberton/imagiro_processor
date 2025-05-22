#pragma once

namespace UtilityProcessorParameters {
  static constexpr const char* PARAMETERS_YAML = R"(

bypass:
  name: "bypass"
  default: 0
  type: toggle

gain:
  name: "gain"
  type: db
  default: 0
  range: {min: -60, max: 12, step: 0.1}

width:
  name: "width"
  type: percent
  default: 1
  range: {min: 0, max: 2, step: 0.01}

mix:
  name: "mix"
  type: percent
  default: 1

)";

};