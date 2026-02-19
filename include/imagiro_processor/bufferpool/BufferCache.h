#pragma once
#include "CacheTypes.h"
#include <immer/map.hpp>
#include <immer/atom.hpp>
#include <atomic>
#include <optional>

namespace imagiro {

// Pure cache management, thread-safe
class BufferCache {
public:
    BufferCache(const uint64_t maxSize = 500 * 1024 * 1024)
        : maxCacheSize(maxSize) {}

    // Try to get a buffer from cache (thread-safe, non-blocking)
    std::optional<CacheEntry> get(size_t keyHash) const {
        auto currentCache = cache.load();
        auto it = currentCache->find(keyHash);
        if (it != nullptr) {
            return *it;
        }
        return std::nullopt;
    }

    // Check if a key exists and is ready (thread-safe, non-blocking)
    bool exists(size_t keyHash) const {
        auto entry = get(keyHash);
        return entry.has_value() && entry->state == CacheEntryState::Ready;
    }

    // Get buffer if ready (thread-safe, non-blocking)
    std::optional<std::shared_ptr<InfoBuffer>> getBuffer(size_t keyHash) const {
        auto entry = get(keyHash);
        if (entry.has_value() && entry->state == CacheEntryState::Ready) {
            return entry->buffer;
        }
        return std::nullopt;
    }

    // Add or update entry (thread-safe)
    void put(size_t keyHash, const CacheEntry& entry) {
        auto currentCache = cache.load();

        // Check if replacing
        auto existing = currentCache->find(keyHash);
        if (existing != nullptr) {
            currentCacheSize.fetch_sub(existing->sizeInBytes);
        }

        // Add new entry
        auto newCache = currentCache->set(keyHash, entry);
        cache.store(newCache);
        currentCacheSize.fetch_add(entry.sizeInBytes);

        // Evict if needed
        while (currentCacheSize.load() > maxCacheSize) {
            evictLRU();
        }
    }

    // Mark entry as loading (thread-safe)
    void markLoading(size_t keyHash) {
        CacheEntry entry;
        entry.state = CacheEntryState::Loading;
        put(keyHash, entry);
    }

    // Mark entry as error (thread-safe)
    void markError(size_t keyHash, const std::string& error) {
        CacheEntry entry;
        entry.state = CacheEntryState::Error;
        entry.errorMessage = error;
        put(keyHash, entry);
    }

    // Clear cache (thread-safe)
    void clear() {
        cache.store({});
        currentCacheSize.store(0);
    }

    size_t getCurrentSize() const { return currentCacheSize.load(); }

private:
    immer::atom<immer::map<size_t, CacheEntry>> cache {};

    std::atomic<size_t> currentCacheSize{0};
    uint64_t maxCacheSize;

    void evictLRU() {
        auto currentCache = cache.load();
        if (currentCache->empty()) return;

        // Find oldest entry that's ready (not loading)
        size_t oldestKey = 0;
        auto oldestTime = std::chrono::steady_clock::now();
        bool found = false;

        for (const auto& [key, entry] : *currentCache) {
            if (entry.state == CacheEntryState::Ready &&
                entry.lastAccess < oldestTime) {
                oldestTime = entry.lastAccess;
                oldestKey = key;
                found = true;
            }
        }

        if (found) {
            auto it = currentCache->find(oldestKey);
            if (it != nullptr) {
                currentCacheSize.fetch_sub(it->sizeInBytes);
                auto newCache = currentCache->erase(oldestKey);
                cache.store(newCache);
            }
        }
    }
};

} // namespace imagiro
