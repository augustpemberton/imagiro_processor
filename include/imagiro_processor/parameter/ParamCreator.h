#pragma once

#include "ParamController.h"

namespace imagiro {
    class ParamCreator {
    public:
        ParamCreator(ParamController& ctrl, std::string prefix)
            : ctrl_(ctrl), prefix_(std::move(prefix)) {}

        Handle add(ParamConfig config) const {
            config.uid = prefix_ + "_" + config.uid;
            return ctrl_.addParam(config);
        }

    private:
        ParamController& ctrl_;
        std::string prefix_;
    };
}