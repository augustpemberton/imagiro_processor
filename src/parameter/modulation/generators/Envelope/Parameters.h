#pragma once

namespace EnvelopeGeneratorParameters {
  static constexpr const char *PARAMETERS_YAML = R"(

attack:
  name: "attack"
  type: time
  default: 0
  range: {min: 0, max: 3, skew: 0.6}

decay:
  name: "decay"
  type: time
  default: 0.4
  range: {min: 0, max: 5, skew: 0.26}

sustain:
  name: "sustain"
  type: db
  default: 0
  range: {min: -60, max: 0, skew: 2}

release:
  name: "release"
  type: time
  default: 0.1
  range: {min: 0, max: 10, skew: 0.3}

)";
};
