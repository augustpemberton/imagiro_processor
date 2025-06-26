//
// Created by August Pemberton on 30/08/2022.
//

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

#include "MipmappedBuffer.h"
#include "imagiro_processor/src/BufferFileLoader.h"
#include "juce_events/juce_events.h"

class GrainBuffer : BufferFileLoader::Listener, juce::Timer {
public:
    GrainBuffer();
    ~GrainBuffer() override;

    std::shared_ptr<MipmappedBuffer<>> getBuffer() const {
        return buffer;
    }

    int getNumChannels() const { return getBuffer()->getBuffer(0)->getNumChannels(); }
    int getNumSamples() const { return getBuffer()->getBuffer(0)->getNumSamples(); }

    void loadFileIntoBuffer(const juce::File &file);
    void clear();

    BufferFileLoader& getFileLoader() { return fileLoader; }

    struct Listener {
        virtual ~Listener() = default;
        virtual void OnBufferUpdated(GrainBuffer&) {}
    };

    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

    juce::File& getLastLoadedFile() { return lastLoadedFile; }

    float getBufferMagnitude() { return bufferMagnitude; }

private:
    juce::File lastRequestedFile;
    juce::File lastLoadedFile;

    juce::ListenerList<Listener> listeners;

    std::shared_ptr<MipmappedBuffer<>> buffer;
    std::vector<std::shared_ptr<MipmappedBuffer<>>> allBuffers;

    std::atomic<float> bufferMagnitude {0};

    void OnFileLoadComplete(std::shared_ptr<juce::AudioSampleBuffer> buffer, double sr, float magnitude) override;
    void OnFileLoadProgress(float progress) override;
    void OnFileLoadError(const juce::String& error) override;

    BufferFileLoader fileLoader;
    void timerCallback() override;
};
