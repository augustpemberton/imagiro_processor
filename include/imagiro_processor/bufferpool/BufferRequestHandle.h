#pragma once
#include "CacheTypes.h"
#include <memory>
#include <optional>
#include <future>

class FileBufferCache;

class BufferRequestHandle : std::enable_shared_from_this<BufferRequestHandle> {
    friend class FileBufferCache;
    friend class BufferRequest;

private:
    FileBufferCache* cache;
    CacheKey key;
    mutable std::shared_ptr<std::promise<Result<std::shared_ptr<InfoBuffer>>>> promise;
    mutable std::shared_future<Result<std::shared_ptr<InfoBuffer>>> future;
    mutable std::atomic<bool> requestStarted{false};

    BufferRequestHandle(FileBufferCache* c, const CacheKey& k);

public:
    // Check if buffer is ready (audio thread safe, non-blocking)
    bool exists() const;

    // Get buffer if ready (audio thread safe, non-blocking)
    std::optional<std::shared_ptr<InfoBuffer>> get() const;

    // Get buffer, blocking until ready or timeout
    Result<std::shared_ptr<InfoBuffer>> getBlocking(int timeoutMs = 10000) const;

    // Check loading state (audio thread safe, non-blocking)
    enum class State {
        NotStarted,
        Loading,
        Ready,
        Error
    };
    State getState() const;

    // Get error message if in error state
    std::optional<std::string> getError() const;
};