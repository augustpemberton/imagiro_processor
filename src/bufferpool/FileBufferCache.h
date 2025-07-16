//
// Created by August Pemberton on 14/07/2025.
//

#pragma once
#include "juce_audio_formats/juce_audio_formats.h"
#include "juce_events/juce_events.h"
#include "../BufferFileLoader.h"
#include <immer/map.hpp>
#include <immer/map_transient.hpp>
#include <immer/atom.hpp>
#include <immer/box.hpp>
#include <memory>
#include <atomic>
#include <queue>

#include "InfoBuffer.h"

struct BufferCacheEntry {
    std::shared_ptr<InfoBuffer> buffer;
    std::atomic<bool> isLoading{false};
    std::atomic<bool> hasError{false};

    BufferCacheEntry() = default;

    BufferCacheEntry(const BufferCacheEntry& other) {
        buffer = other.buffer;
        isLoading.store(other.isLoading.load());
        hasError.store(other.hasError.load());
    }

    BufferCacheEntry& operator=(const BufferCacheEntry& other) {
        buffer = other.buffer;
        isLoading.store(other.isLoading.load());
        hasError.store(other.hasError.load());
        return *this;
    }
};

struct BufferCacheKey {
    std::string path;
    bool normalize;

    bool operator== (const BufferCacheKey& other) const {
        return this->path == other.path &&
               this->normalize == other.normalize;
    }
};

template<>
struct std::hash<BufferCacheKey> {
    std::size_t operator()(const BufferCacheKey& k) const noexcept {
        const std::size_t h1 = std::hash<std::string>{}(k.path);  // FIX: use std::string
        const std::size_t h2 = std::hash<bool>{}(k.normalize);
        return h1 ^ h2 << 1;
    }
};

class FileBufferCache : public juce::Timer, public BufferFileLoader::Listener {
public:
    FileBufferCache();
    ~FileBufferCache() override;

    struct Listener {
        virtual ~Listener() = default;
        virtual void onBufferLoaded(const BufferCacheKey& key, std::shared_ptr<InfoBuffer> buffer) {}
        virtual void onBufferLoadError(const BufferCacheKey& key, const std::string& error) {}
        virtual void onBufferLoadProgress(const BufferCacheKey& key, float progress) {}
    };

    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

    // Get buffer if available, returns nullptr if not loaded (AUDIO THREAD SAFE)
    std::shared_ptr<InfoBuffer> getBuffer(const std::string& filepath, bool normalize = false) const;
    std::shared_ptr<InfoBuffer> getBuffer(const BufferCacheKey& key) const;

    // Get buffer if available, blocking until loaded
    std::shared_ptr<InfoBuffer> getBufferBlocking(const std::string& filepath, bool normalize = false, int timeoutMs = 10000);
    std::shared_ptr<InfoBuffer> getBufferBlocking(const BufferCacheKey& key, int timeoutMs = 10000);

    // Request buffer to be loaded (non-blocking)
    void requestBuffer(const std::string& filepath, bool normalize = false);
    void requestBuffer(const BufferCacheKey& key);

    // Check if buffer is available (AUDIO THREAD SAFE)
    bool isBufferAvailable(const std::string& filepath, bool normalize = false) const;
    bool isBufferAvailable(const BufferCacheKey& key) const;

    // Check if buffer is currently loading (AUDIO THREAD SAFE)
    bool isBufferLoading(const std::string& filepath, bool normalize = false) const;
    bool isBufferLoading(const BufferCacheKey& key) const;

    // Remove buffer from cache
    void removeBuffer(const std::string& filepath);
    void removeBuffer(const BufferCacheKey& key);

    // Clear all buffers
    void clearAll();

    // Timer callback - handles automatic cleanup on message thread
    void timerCallback() override;

    // BufferFileLoader::Listener callbacks
    void OnFileLoadProgress(float progress) override;
    void OnFileLoadComplete(std::shared_ptr<InfoBuffer> buffer) override;
    void OnFileLoadError(const std::string& error) override;

private:
    juce::ListenerList<Listener> listeners;

    // Immutable cache - thread-safe for reads, atomic updates for writes
    immer::atom<immer::map<BufferCacheKey, BufferCacheEntry>> cache;

    // BufferFileLoader for actual loading
    BufferFileLoader loader;
    BufferCacheKey currentLoadingKey;
    std::atomic<bool> isCurrentlyLoading = false;
    std::queue<BufferCacheKey> loadingQueue;
    std::mutex queueMutex;

    // Helper functions
    static BufferCacheKey createKey(const std::string& filepath, bool normalize = false);

    // Message thread operations
    void addToCache(const BufferCacheKey& key, const BufferCacheEntry& entry);
    void markAsLoading(const BufferCacheKey& key);
    void markAsError(const BufferCacheKey& key);

    // Message processing
    void performAutomaticCleanup();
    void processQueue();
    bool tryStartNextLoad();
    void clearQueue();

    std::mutex blockingMutex;
    std::condition_variable blockingCondition;
    std::unordered_map<BufferCacheKey, std::shared_ptr<std::promise<std::shared_ptr<InfoBuffer>>>> blockingPromises;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FileBufferCache)
};