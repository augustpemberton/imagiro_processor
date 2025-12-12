#pragma once

namespace MidiChordProcessorParameters {
  static constexpr const char* PARAMETERS_YAML = R"(

bypass:
  name: "bypass"
  default: 0
  type: toggle

shift:
  name: "shift"
  default: 7
  range: {min: -36, max: 36, step: 1}

strum:
  name: "strum"
  configs:
    free:
      default: 0.2
      type: time
      range: {min: 0, max: 1, skew: 0.3}
    sync:
      range: {type: sync, min: -6, max: 2}
      default: 1
      type: sync
    dotted:
      range: {type: sync, min: -6, max: 2}
      default: 1
      type: sync-dotted
    triplet:
      range: {type: sync, min: -6, max: 2}
      default: 1
      type: sync-triplet


tension:
  name: "tension"
  default: 0
  type: percent
  range: {min: -1, max: 1, step: 0.01}

)";

};