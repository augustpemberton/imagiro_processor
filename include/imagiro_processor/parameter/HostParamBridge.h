#pragma once

#include "imagiro_processor/processor/state/StateRegistry.h"

namespace imagiro {
    class HostParamBridge {
    public:
        virtual ~HostParamBridge() = default;

        virtual void beginGesture(Handle h) = 0;
        virtual void endGesture(Handle h) = 0;
        virtual void pushValueToHost(Handle h, float value01) = 0;
    };
}
