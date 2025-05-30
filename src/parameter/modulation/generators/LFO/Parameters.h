#pragma once

namespace LFOGeneratorParameters {
  static constexpr const char *PARAMETERS_YAML = R"(

frequency:
  name: "rate"
  configs:
    free:
      type: freq
      default: 1
      range: {min: 0, max: 20, skew: 0.45}
    sync:
      range: {type: sync, min: -6, max: 3}
      default: 0.25
      type: sync
      sync: inverse
    dotted:
      range: {type: sync, min: -6, max: 3}
      default: 0.25
      type: sync-dotted
      sync: inverse
    triplet:
      range: {type: sync, min: -6, max: 3}
      default: 0.25
      type: sync-triplet
      sync: inverse

depth:
  name: "depth"
  type: percent
  default: 0.5
  range: {min: 0, max: 1}

phase:
  name: "phase"
  type: degrees
  default: 0
  range: {min: 0, max: 1}

)";
};
