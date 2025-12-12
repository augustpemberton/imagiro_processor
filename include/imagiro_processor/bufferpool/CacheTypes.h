#pragma once
#include "Transform.h"
#include "InfoBuffer.h"
#include <memory>
#include <vector>
#include <chrono>
#include <variant>
#include <future>

#include <expected>

template<typename T>
using Result = std::expected<T, std::string>;

// Cache key representing a transform chain
struct CacheKey {
    std::vector<std::unique_ptr<Transform>> transforms;
    size_t nocacheIndex = std::numeric_limits<size_t>::max();
    
    // Get key for partial chain up to (but not including) index
    CacheKey getPartialKey(size_t upToIndex) const {
        CacheKey partial;
        partial.nocacheIndex = nocacheIndex;
        for (size_t i = 0; i < upToIndex && i < transforms.size(); ++i) {
            partial.transforms.push_back(transforms[i]->clone());
        }
        return partial;
    }
    
    // Get hash for the full chain
    size_t getHash() const {
        return getPartialHash(transforms.size());
    }
    
    // Get hash up to specific transform index
    size_t getPartialHash(size_t upToIndex) const {
        size_t hash = 0;
        for (size_t i = 0; i < upToIndex && i < transforms.size(); ++i) {
            hash ^= transforms[i]->getHash() << (i % 64);
        }
        return hash;
    }
    
    // Copy constructor
    CacheKey(const CacheKey& other) : nocacheIndex(other.nocacheIndex) {
        for (const auto& transform : other.transforms) {
            transforms.push_back(transform->clone());
        }
    }
    
    CacheKey() = default;
    CacheKey(CacheKey&&) = default;
    CacheKey& operator=(CacheKey&&) = default;
    
    CacheKey& operator=(const CacheKey& other) {
        if (this != &other) {
            transforms.clear();
            nocacheIndex = other.nocacheIndex;
            for (const auto& transform : other.transforms) {
                transforms.push_back(transform->clone());
            }
        }
        return *this;
    }
};

// States for cache entries
enum class CacheEntryState {
    Loading,
    Ready,
    Error
};

// Cache entry
struct CacheEntry {
    CacheEntryState state = CacheEntryState::Loading;
    std::shared_ptr<InfoBuffer> buffer;
    std::string errorMessage;
    size_t sizeInBytes = 0;
    std::chrono::steady_clock::time_point lastAccess;
    
    CacheEntry() : lastAccess(std::chrono::steady_clock::now()) {}
    
    void updateAccess() {
        lastAccess = std::chrono::steady_clock::now();
    }
};

// Request tracking for deduplication
struct LoadRequest {
    CacheKey key;
    std::shared_ptr<std::promise<Result<std::shared_ptr<InfoBuffer>>>> promise;
};