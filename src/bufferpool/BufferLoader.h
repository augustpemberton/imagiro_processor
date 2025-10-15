#pragma once
#include "BufferCache.h"
#include <unordered_map>
#include <mutex>
#include <imagiro_util/imagiro_util.h>

class BufferLoader : public juce::Thread {
public:
    BufferLoader(BufferCache& cache);
    ~BufferLoader() override;
    
    // Request a buffer with transform chain
    Result<std::shared_ptr<InfoBuffer>> requestBuffer(const CacheKey& key);

    // Listener interface
    struct Listener {
        virtual ~Listener() = default;
        virtual void onBufferLoaded(const CacheKey& key, std::shared_ptr<InfoBuffer> buffer) {}
        virtual void onBufferLoadError(const CacheKey& key, const std::string& error) {}
    };
    
    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }
    
private:
    BufferCache& cache;
    juce::ListenerList<Listener> listeners;
    
    // Queue of requests
    moodycamel::ReaderWriterQueue<LoadRequest> loadQueue{64};
    
    // Active requests (for deduplication)
    std::mutex activeRequestsMutex;
    std::unordered_map<size_t, std::vector<std::shared_ptr<std::promise<Result<std::shared_ptr<InfoBuffer>>>>>> activeRequests;
    
    // Thread implementation
    void run() override;
    void processRequest(LoadRequest&& request);
    
    // Find the longest cached prefix and return index of next transform to apply
    std::pair<std::shared_ptr<InfoBuffer>, size_t> findCachedPrefix(const CacheKey& key);
    
    // Apply transform chain starting from given index
    Result<std::shared_ptr<InfoBuffer>> applyTransforms(
        std::shared_ptr<InfoBuffer> buffer,
        const CacheKey& key,
        size_t startIndex);
    
    // Calculate buffer metadata
    void updateBufferMetadata(std::shared_ptr<InfoBuffer>& buffer);
    
    // Notify all promises waiting for this key
    void notifyWaiters(size_t keyHash, const Result<std::shared_ptr<InfoBuffer>>& result);
};