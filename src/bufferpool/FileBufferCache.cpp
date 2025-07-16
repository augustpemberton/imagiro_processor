//
// Created by August Pemberton on 14/07/2025.
//

#include "FileBufferCache.h"

FileBufferCache::FileBufferCache() {
    loader.addListener(this);
    startTimerHz(10);
}

FileBufferCache::~FileBufferCache() {
    stopTimer();
    loader.removeListener(this);
}

BufferCacheKey FileBufferCache::createKey(const std::string& filepath, const bool normalize) {
    return BufferCacheKey{
        filepath,
        normalize
    };
}

// TODO this is probably
std::shared_ptr<InfoBuffer> FileBufferCache::getBuffer(const std::string& filepath, const bool normalize) const {
    const auto key = createKey(filepath, normalize);
    return getBuffer(key);
}

std::shared_ptr<InfoBuffer> FileBufferCache::getBuffer(const BufferCacheKey& key) const {
    // AUDIO THREAD SAFE - immutable map, atomic load
    const auto currentCache = *cache.load();
    const auto it = currentCache.find(key);

    if (it != nullptr) {
        if (!it->isLoading && !it->hasError) {
            return it->buffer;
        }
    }
    return nullptr;
}

std::shared_ptr<InfoBuffer> FileBufferCache::getBufferBlocking(const std::string& filepath, const bool normalize, const int timeoutMs) {
    const auto key = createKey(filepath, normalize);
    return getBufferBlocking(key, timeoutMs);
}

std::shared_ptr<InfoBuffer> FileBufferCache::getBufferBlocking(const BufferCacheKey& key, const int timeoutMs) {
    // Check if already available
    if (auto buffer = getBuffer(key)) {
        return buffer;
    }

    // Check if there's an error
    const auto currentCache = *cache.load();
    if (const auto it = currentCache.find(key); it != nullptr && it->hasError) {
        return nullptr;
    }

    // Create promise for this request
    const auto promise = std::make_shared<std::promise<std::shared_ptr<InfoBuffer>>>();
    auto future = promise->get_future();

    {
        std::lock_guard lock(blockingMutex);
        blockingPromises[key] = promise;
    }

    // Request the buffer
    requestBuffer(key);

    // Wait for completion
    const auto status = future.wait_for(std::chrono::milliseconds(timeoutMs));

    {
        std::lock_guard<std::mutex> lock(blockingMutex);
        blockingPromises.erase(key);
    }

    if (status == std::future_status::timeout) {
        return nullptr;
    }

    return future.get();
}

void FileBufferCache::requestBuffer(const std::string& filepath, const bool normalize) {
    const auto key = createKey(filepath, normalize);
    requestBuffer(key);
}

void FileBufferCache::requestBuffer(const BufferCacheKey& key) {
    auto currentCache = *cache.load();
    auto it = currentCache.find(key);

    if (it != nullptr) {
        if (!it->isLoading && !it->hasError) {
            // Already loaded, notify immediately
            listeners.call(&Listener::onBufferLoaded, key, it->buffer);
            return;
        }
        if (it->isLoading) {
            // Already loading, nothing to do
            return;
        }
    } else {
        // Create new entry and mark as loading
        markAsLoading(key);
    }

    // Add to loading queue
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        loadingQueue.push(key);
    }

}

bool FileBufferCache::isBufferAvailable(const std::string& filepath, const bool normalize) const {
    const auto key = createKey(filepath, normalize);
    return isBufferAvailable(key);
}

bool FileBufferCache::isBufferAvailable(const BufferCacheKey& key) const {
    // AUDIO THREAD SAFE
    const auto currentCache = *cache.load();
    const auto it = currentCache.find(key);
    return it != nullptr &&
           !it->isLoading &&
           !it->hasError;
}

bool FileBufferCache::isBufferLoading(const std::string& filepath, const bool normalize) const {
    const auto key = createKey(filepath, normalize);
    return isBufferLoading(key);
}

bool FileBufferCache::isBufferLoading(const BufferCacheKey& key) const {
    const auto currentCache = *cache.load();
    const auto it = currentCache.find(key);
    return it != nullptr && it->isLoading;
}

void FileBufferCache::removeBuffer(const std::string& filepath) {
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    auto currentCache = *cache.load();
    auto transientCache = currentCache.transient();

    // Remove all entries matching filepath
    for (const auto& [key, entry] : currentCache) {
        if (key.path == filepath) {
            transientCache.erase(key);
        }
    }

    cache.store(transientCache.persistent());

    // Also remove from queue
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        std::queue<BufferCacheKey> newQueue;

        while (!loadingQueue.empty()) {
            auto key = loadingQueue.front();
            loadingQueue.pop();

            if (key.path != filepath) {
                newQueue.push(key);
            }
        }

        loadingQueue = std::move(newQueue);
    }
}

void FileBufferCache::removeBuffer(const BufferCacheKey& key) {
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    auto currentCache = *cache.load();
    auto newCache = currentCache.erase(key);
    cache.store(newCache);

    // Also remove from queue
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        std::queue<BufferCacheKey> newQueue;

        while (!loadingQueue.empty()) {
            auto queueKey = loadingQueue.front();
            loadingQueue.pop();

            if (!(queueKey == key)) {
                newQueue.push(queueKey);
            }
        }

        loadingQueue = std::move(newQueue);
    }
}

void FileBufferCache::clearAll() {
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    cache.store(immer::map<BufferCacheKey, BufferCacheEntry>{});
    isCurrentlyLoading = false;
    clearQueue();
}

void FileBufferCache::timerCallback() {
    processQueue();
    // Perform automatic cleanup based on shared_ptr reference counts
    // performAutomaticCleanup();
}

void FileBufferCache::processQueue() {
    tryStartNextLoad();
}

bool FileBufferCache::tryStartNextLoad() {
    // Atomic compare-and-swap to prevent race conditions
    bool expected = false;
    if (!isCurrentlyLoading.compare_exchange_strong(expected, true)) {
        return false; // Already loading
    }

    while (true) {
        BufferCacheKey nextKey;
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (loadingQueue.empty()) {
                isCurrentlyLoading = false; // Reset before returning
                return false; // No more items to process
            }

            nextKey = loadingQueue.front();
            loadingQueue.pop();
        }

        // Verify this item still needs loading
        auto currentCache = *cache.load();
        auto it = currentCache.find(nextKey);

        if (it == nullptr) {
            // Item not in cache, skip
            continue;
        }

        if (!it->isLoading) {
            // Item not marked as loading, skip
            continue;
        }

        if (it->hasError) {
            // Item has error, skip
            continue;
        }

        if (it->buffer) {
            // Item already has buffer, skip
            continue;
        }

        // This item needs loading, proceed
        currentLoadingKey = nextKey;
        // isCurrentlyLoading is already set to true from compare_exchange above

        const auto file = juce::File(currentLoadingKey.path);
        loader.loadFileIntoBuffer(file, currentLoadingKey.normalize);
        return true; // Successfully started loading
    }
}

void FileBufferCache::performAutomaticCleanup() {
    // Remove entries where we're the only ones holding references
    const auto currentCache = *cache.load();
    auto transientCache = currentCache.transient();

    for (const auto& [key, entry] : currentCache) {

        // Keep if currently loading or has error
        if (entry.isLoading || entry.hasError) {
            continue;
        }

        // Remove if buffer is not referenced elsewhere
        if (!entry.buffer || entry.buffer.use_count() <= 1) {
            DBG("Auto-removing unused buffer: " + key.path);
            transientCache.erase(key);
        }
    }

    cache.store(transientCache.persistent());
}

// BufferFileLoader::Listener callbacks
void FileBufferCache::OnFileLoadProgress(float progress) {
    if (isCurrentlyLoading) {
        listeners.call(&Listener::onBufferLoadProgress, currentLoadingKey, progress);
    }
}

void FileBufferCache::OnFileLoadComplete(std::shared_ptr<InfoBuffer> buffer) {
    if (!isCurrentlyLoading) return;

    // Create completed entry
    BufferCacheEntry completedEntry;
    completedEntry.buffer = buffer;
    completedEntry.isLoading = false;
    completedEntry.hasError = false;

    // Update cache atomically
    addToCache(currentLoadingKey, completedEntry);

    // Notify blocking waiters
    {
        std::lock_guard<std::mutex> lock(blockingMutex);
        if (const auto it = blockingPromises.find(currentLoadingKey); it != blockingPromises.end()) {
            it->second->set_value(buffer);
        }
    }

    // Notify listeners
    listeners.call(&Listener::onBufferLoaded, currentLoadingKey, buffer);

    isCurrentlyLoading = false;
}

void FileBufferCache::OnFileLoadError(const std::string& error) {
    if (!isCurrentlyLoading) return;

    markAsError(currentLoadingKey);

    // Notify blocking waiters
    {
        std::lock_guard<std::mutex> lock(blockingMutex);
        if (const auto it = blockingPromises.find(currentLoadingKey); it != blockingPromises.end()) {
            it->second->set_value(nullptr);
        }
    }

    listeners.call(&Listener::onBufferLoadError, currentLoadingKey, error);

    // Mark as not loading - timer will handle starting next load
    isCurrentlyLoading = false;
}

void FileBufferCache::addToCache(const BufferCacheKey& key, const BufferCacheEntry& entry) {
    const auto currentCache = *cache.load();
    auto newCache = currentCache.set(key, entry);
    cache.store(newCache);
}

void FileBufferCache::markAsLoading(const BufferCacheKey& key) {
    BufferCacheEntry loadingEntry;
    loadingEntry.isLoading = true;
    loadingEntry.hasError = false;
    addToCache(key, loadingEntry);
}

void FileBufferCache::markAsError(const BufferCacheKey& key) {
    BufferCacheEntry errorEntry;
    errorEntry.isLoading = false;
    errorEntry.hasError = true;
    addToCache(key, errorEntry);
}

void FileBufferCache::clearQueue() {
    std::lock_guard<std::mutex> lock(queueMutex);
    while (!loadingQueue.empty()) {
        loadingQueue.pop();
    }
}