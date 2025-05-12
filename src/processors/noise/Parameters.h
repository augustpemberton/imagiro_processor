#pragma once

namespace NoiseProcessorParameters {
    static constexpr const char* PARAMETERS_YAML = R"(

    type:
        name: "type"
        internal: true
        type: choice
        choices: ['white', 'file']

    attack:
        name: "attack"
        type: time
        internal: true
        default: 0.05
        range: {min: 0.05, max: 1, skew: 0.3}

    release:
        name: "release"
        type: time
        internal: true
        default: 0.05
        range: {min: 0.05, max: 1, skew: 0.3}

    gain:
      name: "gain"
      type: db
      default: 0
      range: {min: -60, max: 12, step: 0.1}

    lowpass:
        name: "lp"
        type: freq
        default: 10000
        range: {min: 20, max: 20000, type: freq}

    highpass:
        name: "hp"
        type: freq
        default: 300
        range: {min: 20, max: 20000, type: freq}

    )";

};