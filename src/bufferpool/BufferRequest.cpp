#include "BufferRequest.h"
#include "FileBufferCache.h"

BufferRequest& BufferRequest::range(size_t start, size_t end) {
    if (auto* load = dynamic_cast<LoadTransform*>(key.transforms[0].get())) {
        key.transforms[0] = std::make_unique<LoadTransform>(
            load->getFilePath(), start, end, 0);
    }
    return *this;
}

BufferRequest& BufferRequest::channels(int num) {
    if (auto* load = dynamic_cast<LoadTransform*>(key.transforms[0].get())) {
        auto current = std::make_unique<LoadTransform>(*load);
        key.transforms[0] = std::make_unique<LoadTransform>(
            current->getFilePath(),
            current->getStartSample(),
            current->getEndSample(),
            num);
    }
    return *this;
}

BufferRequest& BufferRequest::transform(std::unique_ptr<Transform> t) {
    key.transforms.push_back(std::move(t));
    return *this;
}

BufferRequest& BufferRequest::nocache(size_t fromIndex) {
    key.nocacheIndex = fromIndex;
    return *this;
}

BufferRequestHandle BufferRequest::execute() {
    return cache->createHandle(key);
}

Result<std::shared_ptr<InfoBuffer>> BufferRequest::executeBlocking() {
    return cache->requestBuffer(key);
}
