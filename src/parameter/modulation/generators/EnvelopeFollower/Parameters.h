#pragma once

namespace EnvelopeFollowerGeneratorParameters {
  static constexpr const char *PARAMETERS_YAML = R"(

attack:
  name: "attack"
  type: time
  default: 0.01
  range: {min: 0, max: 1, skew: 0.5}

release:
  name: "release"
  type: time
  default: 0.5
  range: {min: 0, max: 4, skew: 0.5}

source:
  name: "source"
  type: choice
  choices: []

)";
};
