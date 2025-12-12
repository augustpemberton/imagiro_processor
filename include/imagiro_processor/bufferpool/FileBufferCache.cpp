#include "FileBufferCache.h"
#include "BufferRequestHandle.h"

FileBufferCache::FileBufferCache(uint64_t maxCacheSize) {
    cache = std::make_unique<BufferCache>(maxCacheSize);
    loader = std::make_unique<BufferLoader>(*cache);
    afm.registerBasicFormats();
}

FileBufferCache::~FileBufferCache() = default;

BufferRequestHandle FileBufferCache::createHandle(const CacheKey& key) {
    return BufferRequestHandle(this, key);
}

std::optional<std::shared_ptr<InfoBuffer>> FileBufferCache::getBuffer(const CacheKey& key) {
    return cache->getBuffer(key.getHash());
}

Result<std::shared_ptr<InfoBuffer>> FileBufferCache::requestBuffer(const CacheKey& key) {
    return loader->requestBuffer(key);
}