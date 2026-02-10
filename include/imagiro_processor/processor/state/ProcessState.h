// ProcessState.h
#pragma once

#include "imagiro_processor/parameter/ParamValue.h"
#include "StateRegistry.h"
#include <vector>

namespace imagiro {

    class ProcessState {
    public:
        std::vector<ParamValue>& params() { return params_; }
        const std::vector<ParamValue>& params() const { return params_; }

        float value01(Handle h) const { return params_[h.index].value01; }
        float userValue(Handle h) const { return params_[h.index].userValue; }

        float value(Handle h) const {
            const auto& pv = params_[h.index];
            return pv.toProcessor
                ? pv.toProcessor(pv.userValue, bpm_, sampleRate_)
                : pv.userValue;
        }

        double bpm() const { return bpm_; }
        double sampleRate() const { return sampleRate_; }

        void setBpm(double bpm) { bpm_ = bpm; }
        void setSampleRate(double sr) { sampleRate_ = sr; }

    private:
        std::vector<ParamValue> params_;
        double bpm_{120.0};
        double sampleRate_{44100.0};
    };

} // namespace imagiro
