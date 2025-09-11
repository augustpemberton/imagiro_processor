#include "BufferRequestHandle.h"
#include "FileBufferCache.h"

BufferRequestHandle::BufferRequestHandle(FileBufferCache* c, const CacheKey& k)
    : cache(c), key(k) {
    // Create promise/future pair
    promise = std::make_shared<std::promise<Result<std::shared_ptr<InfoBuffer>>>>();
    future = promise->get_future().share();

    // Start loading asynchronously on message thread
    juce::MessageManager::callAsync([p = this->promise, c, k]() {
        auto result = c->requestBuffer(k);
        try {
            p->set_value(result.value());
        } catch (...) {
            p->set_value(Result<std::shared_ptr<InfoBuffer>>::unexpected_type("Exception during loading"));
        }
    });
}

bool BufferRequestHandle::exists() const {
    // This only reads from the lock-free cache
    return cache->cache->exists(key.getHash());
}

std::optional<std::shared_ptr<InfoBuffer>> BufferRequestHandle::get() const {
    // This only reads from the lock-free cache
    return cache->getBuffer(key);
}

Result<std::shared_ptr<InfoBuffer>> BufferRequestHandle::getBlocking(int timeoutMs) const {
    // First check if already available (lock-free)
    if (auto buffer = get()) {
        return *buffer;
    }

    // Wait on future with timeout
    if (future.valid()) {
        auto status = future.wait_for(std::chrono::milliseconds(timeoutMs));

        if (status == std::future_status::timeout) {
            return Result<std::shared_ptr<InfoBuffer>>::unexpected_type("Timeout waiting for buffer");
        }

        return future.get();
    }

    return Result<std::shared_ptr<InfoBuffer>>::unexpected_type("No active request");
}

BufferRequestHandle::State BufferRequestHandle::getState() const {
    auto entry = cache->cache->get(key.getHash());

    if (!entry.has_value()) {
        return State::NotStarted;
    }

    switch (entry->state) {
        case CacheEntryState::Loading:
            return State::Loading;
        case CacheEntryState::Ready:
            return State::Ready;
        case CacheEntryState::Error:
            return State::Error;
    }

    return State::NotStarted;
}

std::optional<std::string> BufferRequestHandle::getError() const {
    auto entry = cache->cache->get(key.getHash());

    if (entry.has_value() && entry->state == CacheEntryState::Error) {
        return entry->errorMessage;
    }

    return std::nullopt;
}