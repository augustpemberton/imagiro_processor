//
// Created by August Pemberton on 14/07/2025.
//

#pragma once
#include "juce_audio_formats/juce_audio_formats.h"
#include "juce_events/juce_events.h"
#include "../BufferFileLoader.h"
#include <immer/map.hpp>
#include <immer/map_transient.hpp>
#include <memory>
#include <atomic>

#include "InfoBuffer.h"
#include "immer/atom.hpp"
#include "immer/box.hpp"

struct BufferCacheEntry {
    std::shared_ptr<InfoBuffer> buffer;
    std::atomic<bool> isLoading{false};
    std::atomic<bool> hasError{false};

    BufferCacheEntry() = default;

    BufferCacheEntry(const BufferCacheEntry& other) {
        buffer = other.buffer;
        isLoading = other.isLoading.load();
        hasError = other.hasError.load();
    }

    BufferCacheEntry& operator=(const BufferCacheEntry& other) {
        buffer = other.buffer;
        isLoading = other.isLoading.load();
        hasError = other.hasError.load();
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
        const std::size_t h1 = std::hash<juce::String>{}(k.path);
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
        virtual void onBufferLoadError(const BufferCacheKey& key, const juce::String& error) {}
        virtual void onBufferLoadProgress(const BufferCacheKey& key, float progress) {}
    };

    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

    // Get buffer if available, returns nullptr if not loaded (AUDIO THREAD SAFE)
    std::shared_ptr<InfoBuffer> getBuffer(const juce::String& filepath, bool normalize = false) const;
    std::shared_ptr<InfoBuffer> getBuffer(const BufferCacheKey& key) const;

    // Request buffer to be loaded (non-blocking)
    void requestBuffer(const juce::String& filepath, bool normalize = false);
    void requestBuffer(const BufferCacheKey& key);

    // Check if buffer is available (AUDIO THREAD SAFE)
    bool isBufferAvailable(const juce::String& filepath, bool normalize = false) const;
    bool isBufferAvailable(const BufferCacheKey& key) const;

    // Check if buffer is currently loading (AUDIO THREAD SAFE)
    bool isBufferLoading(const juce::String& filepath, bool normalize = false) const;
    bool isBufferLoading(const BufferCacheKey& key) const;

    // Remove buffer from cache
    void removeBuffer(const juce::String& filepath);
    void removeBuffer(const BufferCacheKey& key);

    // Clear all buffers
    void clearAll();

    // Timer callback - handles automatic cleanup on message thread
    void timerCallback() override;

    // BufferFileLoader::Listener callbacks
    void OnFileLoadProgress(float progress) override;
    void OnFileLoadComplete(std::shared_ptr<InfoBuffer> buffer) override;
    void OnFileLoadError(const juce::String& error) override;

private:
    juce::ListenerList<Listener> listeners;

    // Immutable cache - thread-safe for reads, atomic updates for writes
    immer::atom<immer::map<BufferCacheKey, BufferCacheEntry>> cache;

    // BufferFileLoader for actual loading
    BufferFileLoader loader;
    BufferCacheKey currentLoadingKey;
    bool isCurrentlyLoading = false;

    // Helper functions
    static BufferCacheKey createKey(const juce::String& filepath, bool normalize = false);

    // Message thread operations
    void addToCache(const BufferCacheKey& key, const BufferCacheEntry& entry);
    void markAsLoading(const BufferCacheKey& key);
    void markAsError(const BufferCacheKey& key);

    // Message processing
    void performAutomaticCleanup();
    void startNextLoad();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FileBufferCache)
};