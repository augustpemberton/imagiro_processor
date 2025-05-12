#pragma once

namespace SaturationProcessorParameters {
    static constexpr const char* PARAMETERS_YAML = R"(

    drive:
      name: "drive"
      type: db
      default: 0
      range: {min: -36, max: 36, step: 0.1}

    mix:
      name: "mix"
      type: percent
      default: 1

    )";

};