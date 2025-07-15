//
// Created by August Pemberton on 14/07/2025.
//

#include "FileBufferCache.h"

FileBufferCache::FileBufferCache() {
    loader.addListener(this);
    startTimerHz(1);
}

FileBufferCache::~FileBufferCache() {
    stopTimer();
    loader.removeListener(this);
}

BufferCacheKey FileBufferCache::createKey(const juce::String& filepath, const bool normalize) {
    const auto file = juce::File(filepath);
    return BufferCacheKey{
        file.getFullPathName().toStdString(),
        normalize
    };
}

std::shared_ptr<InfoBuffer> FileBufferCache::getBuffer(const juce::String& filepath, const bool normalize) const {
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

void FileBufferCache::requestBuffer(const juce::String& filepath, const bool normalize) {
    const auto key = createKey(filepath, normalize);
    requestBuffer(key);
}

void FileBufferCache::requestBuffer(const BufferCacheKey& key) {
    // jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

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

    // Start loading immediately if not currently loading
    if (!isCurrentlyLoading) {
        startNextLoad();
    }
}

bool FileBufferCache::isBufferAvailable(const juce::String& filepath, const bool normalize) const {
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

bool FileBufferCache::isBufferLoading(const juce::String& filepath, const bool normalize) const {
    const auto key = createKey(filepath, normalize);
    return isBufferLoading(key);
}

bool FileBufferCache::isBufferLoading(const BufferCacheKey& key) const {
    const auto currentCache = *cache.load();
    const auto it = currentCache.find(key);
    return it != nullptr && it->isLoading;
}

void FileBufferCache::removeBuffer(const juce::String& filepath) {
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
}

void FileBufferCache::removeBuffer(const BufferCacheKey& key) {
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    auto currentCache = *cache.load();
    auto newCache = currentCache.erase(key);
    cache.store(newCache);
}

void FileBufferCache::clearAll() {
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    cache.store(immer::map<BufferCacheKey, BufferCacheEntry>{});
    isCurrentlyLoading = false;
}

void FileBufferCache::timerCallback() {
    // Perform automatic cleanup based on shared_ptr reference counts
    // performAutomaticCleanup();
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

void FileBufferCache::startNextLoad() {
    // Find next item that needs loading
    auto currentCache = cache.load();

    for (const auto& [key, val] : *currentCache) {
        const auto& entry = val;
        if (entry.isLoading.load() && !entry.hasError.load() && !entry.buffer) {
            // Found something to load
            currentLoadingKey = key;
            isCurrentlyLoading = true;

            // Start loading with existing loader
            const auto file = juce::File(currentLoadingKey.path);
            loader.loadFileIntoBuffer(file, currentLoadingKey.normalize);
            return;
        }
    }
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

    // Notify listeners
    listeners.call(&Listener::onBufferLoaded, currentLoadingKey, buffer);

    // Clean up and start next load
    isCurrentlyLoading = false;
    startNextLoad();
}

void FileBufferCache::OnFileLoadError(const juce::String& error) {
    if (!isCurrentlyLoading) return;

    markAsError(currentLoadingKey);
    listeners.call(&Listener::onBufferLoadError, currentLoadingKey, error);

    // Clean up and start next load
    isCurrentlyLoading = false;
    startNextLoad();
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
