#pragma once

#include <atomic>
#include <memory>

namespace imagiro {

// std::atomic<std::shared_ptr<T>> where the standard library implements the
// C++20 specialization (libstdc++); Apple's libc++ doesn't ship it yet, so
// fall back to the pre-C++20 atomic free functions there.
#if defined(__cpp_lib_atomic_shared_ptr)

template <typename T>
using AtomicSharedPtr = std::atomic<std::shared_ptr<T>>;

#else

template <typename T>
class AtomicSharedPtr {
public:
    std::shared_ptr<T> load(std::memory_order order = std::memory_order_seq_cst) const {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        return std::atomic_load_explicit(&ptr_, order);
#pragma clang diagnostic pop
    }

    void store(std::shared_ptr<T> p, std::memory_order order = std::memory_order_seq_cst) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        std::atomic_store_explicit(&ptr_, std::move(p), order);
#pragma clang diagnostic pop
    }

private:
    std::shared_ptr<T> ptr_;
};

#endif

} // namespace imagiro
