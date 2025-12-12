#pragma once
#include "CacheTypes.h"
#include "BufferRequestHandle.h"
#include <memory>
#include <string>

#include "CommonTransforms.h"

class FileBufferCache;

// Fluent API for building buffer requests
class BufferRequest {
    friend class FileBufferCache;

private:
    FileBufferCache* cache;
    CacheKey key;

    BufferRequest(FileBufferCache* c, const std::string& path);

public:
    // Set sample range
    BufferRequest& range(size_t start, size_t end);

    // Set number of channels
    BufferRequest& channels(int num);

    // Add a transform
    BufferRequest& transform(std::unique_ptr<Transform> t);

    // Set nocache index (transforms after this won't be cached)
    BufferRequest& nocache(size_t fromIndex);

    // Execute and get handle
    BufferRequestHandle execute();

    // Execute and get result directly (blocking)
    Result<std::shared_ptr<InfoBuffer>> executeBlocking();
};