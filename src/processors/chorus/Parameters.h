#pragma once

namespace ChorusProcessorParameters {
    static constexpr const char* PARAMETERS_YAML = R"(

    rate:
      name: "rate"
      type: freq
      default: 0.3
      range: {min: 0.1, max: 10.0, step: 0.01, skew: 0.3}
      
    depth:
      name: "depth"
      type: percent
      default: 0.3
      range: {min: 0, max: 1, step: 0.01, skew: 0.5}

    feedback:
      name: "feedback"
      type: percent
      default: 0
      range: {min: 0, max: 1, step: 0.01, skew: 0.5}

    voices:
      name: "voices"
      default: 2
      range: {min: 1, max: 8, step: 1}

    mix:
      name: "mix"
      type: percent
      default: 0.4
      range: {min: 0, max: 1, step: 0.01}

    bypass:
      name: "bypass"
      default: 0
      type: toggle

    )";
};