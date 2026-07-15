#pragma once

#include <atomic>
#include <memory>

namespace imagiro {

// Atomic shared_ptr slot via the C++11 atomic free functions. Apple's libc++
// doesn't implement std::atomic<std::shared_ptr<T>> yet; when it does, this can
// become a plain alias for it. The free functions are deprecated in C++20
// (removed in C++26) but implemented everywhere.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

template <typename T>
class AtomicSharedPtr {
public:
    AtomicSharedPtr() = default;
    AtomicSharedPtr(std::shared_ptr<T> p) : ptr_(std::move(p)) {}

    std::shared_ptr<T> load(std::memory_order order = std::memory_order_seq_cst) const {
        return std::atomic_load_explicit(&ptr_, order);
    }

    void store(std::shared_ptr<T> p, std::memory_order order = std::memory_order_seq_cst) {
        std::atomic_store_explicit(&ptr_, std::move(p), order);
    }

private:
    std::shared_ptr<T> ptr_;
};

#pragma GCC diagnostic pop

} // namespace imagiro
