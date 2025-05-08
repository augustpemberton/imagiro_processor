#pragma once

namespace GainProcessorParameters {
    static constexpr const char* PARAMETERS_YAML = R"(

    gain:
      name: "gain"
      type: db
      default: 0
      range: {min: -60, max: 12, step: 0.1}

    )";

};