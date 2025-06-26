//
// Created by August Pemberton on 21/01/2025.
//

#include "GrainBuffer.h"

GrainBuffer::GrainBuffer() {
    fileLoader.addListener(this);
    startTimerHz(5);
    clear();
}

GrainBuffer::~GrainBuffer() {
    stopTimer();
    fileLoader.removeListener(this);
}

void GrainBuffer::loadFileIntoBuffer(const juce::File &file) {
    fileLoader.loadFileIntoBuffer(file, true);
    lastRequestedFile = file;
}

void GrainBuffer::clear() {
    lastLoadedFile = "";
    bufferMagnitude = 0;
    this->buffer = nullptr;
    listeners.call(&Listener::OnBufferUpdated, *this);
}

void GrainBuffer::OnFileLoadComplete(std::shared_ptr<juce::AudioSampleBuffer> buffer, double sr, const float magnitude) {
    DBG("file load complete");
    lastLoadedFile = lastRequestedFile;
    bufferMagnitude = magnitude;

    this->buffer = std::make_shared<MipmappedBuffer<>>(buffer, sr);
    allBuffers.push_back(this->buffer);

    listeners.call(&Listener::OnBufferUpdated, *this);
}

void GrainBuffer::OnFileLoadProgress(float progress) {
    //
}

void GrainBuffer::OnFileLoadError(const juce::String& error) {
    DBG("file load error: " + error);
}

void GrainBuffer::timerCallback() {
    for (auto it = allBuffers.begin(); it != allBuffers.end();) {
        // if only one reference, we are the only holder, safe to delete
        if (it->use_count() == 1) {
            DBG("de-allocating buffer");
            it = allBuffers.erase(it);
        } else ++it;
    }
}
