#pragma once
#include "BufferCache.h"
#include "BufferLoader.h"
#include "BufferRequest.h"
#include <memory>

// Main interface combining cache and loader
class FileBufferCache {
public:
    FileBufferCache(size_t maxCacheSize = 500 * 1024 * 1024);
    ~FileBufferCache();

    // Fluent API entry point
    BufferRequest request(const std::string& path) {
        return BufferRequest(this, path);
    }

    // Cache management
    void setMaxCacheSize(size_t bytes) {
        cache = std::make_unique<BufferCache>(bytes);
    }
    void clearCache() { cache->clear(); }
    size_t getCurrentCacheSize() const { return cache->getCurrentSize(); }

    // Listener interface (forwarded from loader)
    using Listener = BufferLoader::Listener;
    void addListener(Listener* l) { loader->addListener(l); }
    void removeListener(Listener* l) { loader->removeListener(l); }

private:
    friend class BufferRequest;
    friend class BufferRequestHandle;

    std::unique_ptr<BufferCache> cache;
    std::unique_ptr<BufferLoader> loader;

    // Internal methods for BufferRequest/Handle
    BufferRequestHandle createHandle(const CacheKey& key);
    std::optional<std::shared_ptr<InfoBuffer>> getBuffer(const CacheKey& key);
    Result<std::shared_ptr<InfoBuffer>> requestBuffer(const CacheKey& key);
};