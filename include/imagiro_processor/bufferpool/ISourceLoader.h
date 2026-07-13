#pragma once

#include "InfoBuffer.h"
#include <imagiro_util/fs.h>
#include <functional>
#include <memory>

namespace imagiro {

// Abstracts file → InfoBuffer loading so consumers stay framework-free.
// load() blocks until the buffer is ready, polling shouldCancel while waiting;
// returns nullptr on cancellation or load failure.
struct ISourceLoader {
    virtual ~ISourceLoader() = default;
    virtual std::shared_ptr<InfoBuffer> load(
        const imagiro::fs::path& path,
        const std::function<bool()>& shouldCancel) = 0;
};

} // namespace imagiro

namespace novo {
using ISourceLoader = imagiro::ISourceLoader;
}
