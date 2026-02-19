#include "FileBufferCache.h"
#include "BufferRequestHandle.h"

namespace imagiro {

FileBufferCache::FileBufferCache(uint64_t maxCacheSize) {
    cache = std::make_unique<BufferCache>(maxCacheSize);
    loader = std::make_unique<BufferLoader>(*cache);
    afm.registerBasicFormats();
}

FileBufferCache::~FileBufferCache() = default;

std::shared_ptr<BufferRequestHandle> FileBufferCache::createHandle(const CacheKey& key) {
    return std::shared_ptr<BufferRequestHandle>(new BufferRequestHandle(this, key));
}

std::optional<std::shared_ptr<InfoBuffer>> FileBufferCache::getBuffer(const CacheKey& key) {
    return cache->getBuffer(key.getHash());
}

Result<std::shared_ptr<InfoBuffer>> FileBufferCache::requestBuffer(const CacheKey& key) {
    return loader->requestBuffer(key);
}

} // namespace imagiro
