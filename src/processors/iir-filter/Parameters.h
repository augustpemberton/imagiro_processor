#pragma once

namespace IIRFilterProcessorParameters {
    static constexpr const char* PARAMETERS_YAML = R"(

    frequency:
        name: "frequency"
        type: freq
        default: 1000
        range: {min: 20, max: 20000, type: freq}

    type:
        name: "filter type"
        type: choice
        choices: ['lowpass', 'bandpass', 'highpass']
        default: 0

    mix:
      name: "mix"
      type: percent
      default: 1


    )";

};