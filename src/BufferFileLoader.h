//
// Created by August Pemberton on 21/01/2025.
//

#pragma once
#include "bufferpool/InfoBuffer.h"
#include "juce_audio_formats/juce_audio_formats.h"


class BufferFileLoader : public juce::Thread, juce::Timer {
public:

    BufferFileLoader();

    ~BufferFileLoader() override;

    /*
     * Loads a file into the given buffer on a background thread.
     * Note - this will resize the given buffer to match the file.
     */
    void loadFileIntoBuffer(const juce::File &fileToLoad, bool normalizeFile);

    struct Listener {
        virtual ~Listener() = default;

        virtual void OnFileLoadProgress(float progress) {}
        virtual void OnFileLoadComplete(std::shared_ptr<InfoBuffer> buffer) {}
        virtual void OnFileLoadError(const juce::String& error) {}
    };

    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

private:
    juce::AudioFormatManager afm;
    juce::ListenerList<Listener> listeners;

    juce::File fileToLoad;
    bool normalizeFile {false};

    const int oversampleRatio = 1;

    std::shared_ptr<InfoBuffer> loadedBuffer {nullptr};
    std::atomic<bool> loadCompleteFlag {false};

    std::atomic<float> progress;
    std::atomic<bool> loadProgressFlag;
    void timerCallback() override;

    void run() override;
};
