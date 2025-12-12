#pragma once
#include "InfoBuffer.h"
#include <memory>
#include <atomic>
#include <chrono>

struct CacheEntry {
    std::shared_ptr<InfoBuffer> buffer;
    std::atomic<bool> isLoading{false};
    std::atomic<bool> hasError{false};
    std::chrono::steady_clock::time_point lastAccess;
    size_t sizeInBytes = 0;
    
    CacheEntry() : lastAccess(std::chrono::steady_clock::now()) {}
    
    CacheEntry(const CacheEntry& other) {
        buffer = other.buffer;
        isLoading.store(other.isLoading.load());
        hasError.store(other.hasError.load());
        lastAccess = other.lastAccess;
        sizeInBytes = other.sizeInBytes;
    }
    
    CacheEntry& operator=(const CacheEntry& other) {
        buffer = other.buffer;
        isLoading.store(other.isLoading.load());
        hasError.store(other.hasError.load());
        lastAccess = other.lastAccess;
        sizeInBytes = other.sizeInBytes;
        return *this;
    }
    
    void updateAccess() {
        lastAccess = std::chrono::steady_clock::now();
    }
};