#include "BufferLoader.h"

BufferLoader::BufferLoader(BufferCache& cache) 
    : juce::Thread("BufferLoader"), cache(cache) {
    startThread();
}

BufferLoader::~BufferLoader() {
    stopThread(4000);
}

Result<std::shared_ptr<InfoBuffer>> BufferLoader::requestBuffer(const CacheKey& key) {
    auto promise = std::make_shared<std::promise<Result<std::shared_ptr<InfoBuffer>>>>();
    auto future = promise->get_future();

    const size_t keyHash = key.getHash();

    // Check if already in cache
    if (auto entry = cache.get(keyHash)) {
        if (entry->state == CacheEntryState::Ready) {
            listeners.call(&Listener::onBufferLoaded, key, entry->buffer);
            return entry->buffer;
        } else if (entry->state == CacheEntryState::Error) {
            return Result<std::shared_ptr<InfoBuffer>>::unexpected_type(entry->errorMessage);
        }
        // If loading, add to waiters below
    }

    // Check if already loading
    {
        std::lock_guard<std::mutex> lock(activeRequestsMutex);
        auto it = activeRequests.find(keyHash);
        if (it != activeRequests.end()) {
            // Already loading, add to waiters
            it->second.push_back(promise);
            return future.get(); // Block until ready
        }

        // Mark as loading
        activeRequests[keyHash] = {promise};
    }

    // Mark in cache as loading
    cache.markLoading(keyHash);

    // Queue request
    LoadRequest request{key, promise};
    loadQueue.enqueue(std::move(request));
    notify();

    return future.get(); // Block until ready
}

void BufferLoader::run() {
    while (!threadShouldExit()) {
        LoadRequest request;
        if (loadQueue.try_dequeue(request)) {
            processRequest(std::move(request));
        } else {
            wait(100);
        }
    }
}

void BufferLoader::processRequest(LoadRequest&& request) {
    const size_t keyHash = request.key.getHash();
    
    // Find longest cached prefix
    auto [startBuffer, startIndex] = findCachedPrefix(request.key);
    
    Result<std::shared_ptr<InfoBuffer>> result;
    
    if (!startBuffer && startIndex == 0) {
        // Need to load from file - check if first transform is LoadTransform
        if (request.key.transforms.empty() || 
            !dynamic_cast<LoadTransform*>(request.key.transforms[0].get())) {
            result = Result<std::shared_ptr<InfoBuffer>>::unexpected_type("No LoadTransform found at start of chain");
        } else {
            // Create empty buffer for LoadTransform to fill
            auto buffer = std::make_shared<InfoBuffer>();
            buffer->buffer = juce::AudioSampleBuffer();
            double sampleRate = 0;
            
            if (request.key.transforms[0]->process(buffer->buffer, sampleRate)) {
                buffer->sampleRate = sampleRate;
                updateBufferMetadata(buffer);
                
                // Cache the loaded buffer
                if (request.key.nocacheIndex > 0) {
                    CacheEntry entry;
                    entry.state = CacheEntryState::Ready;
                    entry.buffer = buffer;
                    entry.sizeInBytes = buffer->buffer.getNumSamples() * 
                                       buffer->buffer.getNumChannels() * 
                                       sizeof(float);
                    cache.put(request.key.getPartialHash(1), entry);
                }
                
                // Apply remaining transforms
                result = applyTransforms(buffer, request.key, 1);
            } else {
                result = Result<std::shared_ptr<InfoBuffer>>::unexpected_type(request.key.transforms[0]->getLastError());
            }
        }
    } else if (startBuffer) {
        // Apply remaining transforms
        result = applyTransforms(startBuffer, request.key, startIndex);
    } else {
        result = Result<std::shared_ptr<InfoBuffer>>::unexpected_type("Failed to find valid starting point");
    }
    
    // Update cache with final result
    if (result.has_value()) {
        CacheEntry entry;
        entry.state = CacheEntryState::Ready;
        entry.buffer = result.value();
        entry.sizeInBytes = entry.buffer->buffer.getNumSamples() * 
                           entry.buffer->buffer.getNumChannels() * 
                           sizeof(float);
        cache.put(keyHash, entry);
        
        // Notify listeners
        listeners.call(&Listener::onBufferLoaded, request.key, entry.buffer);
    } else {
        cache.markError(keyHash, result.error());
        listeners.call(&Listener::onBufferLoadError, request.key, result.error());
    }
    
    // Notify all waiters
    notifyWaiters(keyHash, result);
}

std::pair<std::shared_ptr<InfoBuffer>, size_t> BufferLoader::findCachedPrefix(const CacheKey& key) {
    // Search from the end backwards to find longest cached chain
    for (size_t i = key.transforms.size(); i > 0; --i) {
        if (auto buffer = cache.getBuffer(key.getPartialHash(i))) {
            return {*buffer, i};
        }
    }
    return {nullptr, 0};
}

Result<std::shared_ptr<InfoBuffer>> BufferLoader::applyTransforms(
    std::shared_ptr<InfoBuffer> startBuffer,
    const CacheKey& key,
    size_t startIndex) {
    
    // Make a copy to work with
    auto workingBuffer = std::make_shared<InfoBuffer>(*startBuffer);
    
    // Apply each transform
    for (size_t i = startIndex; i < key.transforms.size(); ++i) {
        double sampleRate = workingBuffer->sampleRate;
        
        if (!key.transforms[i]->process(workingBuffer->buffer, sampleRate)) {
            return  Result<std::shared_ptr<InfoBuffer>>::unexpected_type(key.transforms[i]->getLastError());
        }
        
        workingBuffer->sampleRate = sampleRate;
        updateBufferMetadata(workingBuffer);
        
        // Cache intermediate result if before nocache index
        if (i + 1 <= key.nocacheIndex && i + 1 < key.transforms.size()) {
            auto bufferCopy = std::make_shared<InfoBuffer>(*workingBuffer);
            
            CacheEntry entry;
            entry.state = CacheEntryState::Ready;
            entry.buffer = bufferCopy;
            entry.sizeInBytes = bufferCopy->buffer.getNumSamples() * 
                               bufferCopy->buffer.getNumChannels() * 
                               sizeof(float);
            
            cache.put(key.getPartialHash(i + 1), entry);
        }
    }
    
    return workingBuffer;
}

void BufferLoader::updateBufferMetadata(std::shared_ptr<InfoBuffer>& buffer) {
    buffer->maxMagnitude = buffer->buffer.getMagnitude(0, buffer->buffer.getNumSamples());
}

void BufferLoader::notifyWaiters(size_t keyHash, const Result<std::shared_ptr<InfoBuffer>>& result) {
    std::vector<std::shared_ptr<std::promise<Result<std::shared_ptr<InfoBuffer>>>>> promises;
    
    {
        std::lock_guard<std::mutex> lock(activeRequestsMutex);
        auto it = activeRequests.find(keyHash);
        if (it != activeRequests.end()) {
            promises = std::move(it->second);
            activeRequests.erase(it);
        }
    }
    
    // Notify all promises
    for (auto& promise : promises) {
        promise->set_value(result);
    }
}