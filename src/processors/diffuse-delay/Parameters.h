#pragma once

namespace DiffuseDelayProcessorParameters {
    static constexpr const char *PARAMETERS_YAML = R"(

mix:
  name: "mix"
  type: percent
  default: 0.5
diffuse:
  name: "diffuse"
  type: percent
  default: 0.2
  range: {min: 0, max: 1, step: 0.0001}

diffuseModDepth:
  name: "mod depth"
  type: percent
  default: 0.1
  range: {min: 0, max: 1, step: 0.0001}

diffuseModRate:
  name: "mod rate"
  type: freq
  default: 2
  range: {min: 0, max: 20, step: 0.01, skew: 0.5}

diffuseLowpass:
  name: "lowpass"
  type: freq
  default: 4000
  range: {min: 20, max: 20000, type: freq}

diffuseHighpass:
  name: "highpass"
  type: freq
  default: 100
  range: {min: 20, max: 20000, type: freq}

feedback:
  name: "feedback"
  type: percent
  default: 0.5
  range: {min: 0, max: 1, step: 0.01}

delay:
  name: "delay"
  configs:
    free:
      range: { min: 0.01, max: 5, step: 0.001, skew: 0.3 }
      default: 0.25
      type: time
    sync:
      range: {type: sync, min: -6, max: 3} # sync ranges are defined in powers of 2
      default: 1
      type: sync
    dotted:
      range: {type: sync, min: -6, max: 3}
      default: 1
      type: sync-dotted
    triplet:
      range: {type: sync, min: -6, max: 3}
      default: 1
      type: sync-triplet


    )";
};
