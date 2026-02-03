//
// ParamMember.h - Declarative parameter member for automatic registration
//

#pragma once

#include "ParamConfig.h"
#include "ParamController.h"

namespace imagiro {

/**
 * A parameter that self-registers when declared as a class member.
 *
 * Usage:
 *   class MyProcessor : public Processor {
 *       ParamMember gainParam{params(), {.uid = "gain", .name = "gain", ...}};
 *       ParamMember toneParam{params(), {.uid = "tone", .name = "tone", ...}};
 *
 *       // Objects declared after params can safely use them
 *       MyHelper helper{*this};  // helper can access gainParam, toneParam
 *   };
 *
 * C++ guarantees member initialization in declaration order, so any objects
 * declared after ParamMember instances will see valid handles.
 */
class ParamMember {
public:
    ParamMember(ParamController& controller, ParamConfig config)
        : handle_(controller.addParam(std::move(config))) {}

    // Implicit conversion to Handle for seamless use
    operator Handle() const { return handle_; }

    // Explicit access
    Handle handle() const { return handle_; }

    // Check validity
    bool isValid() const { return handle_.index != 0xFFFF; }

private:
    Handle handle_;
};

} // namespace imagiro
