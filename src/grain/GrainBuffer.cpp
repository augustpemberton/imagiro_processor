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

void GrainBuffer::loadFileIntoBuffer(juce::File file, double targetSampleRate) {
    fileLoader.loadFileIntoBuffer(file, targetSampleRate, true);
    lastRequestedFile = file;
}

void GrainBuffer::clear() {
    lastLoadedFile = "";
    bufferMagnitude = 0;
    this->buffer = std::make_shared<juce::AudioSampleBuffer>(0, 0);
    listeners.call(&Listener::OnBufferUpdated, *this);
}

void GrainBuffer::OnFileLoadComplete(std::shared_ptr<juce::AudioSampleBuffer> buffer, float mag) {
    DBG("file load complete");
    lastLoadedFile = lastRequestedFile;
    bufferMagnitude = mag;
    this->buffer = buffer;
    allBuffers.push_back(buffer);
    listeners.call(&Listener::OnBufferUpdated, *this);
}

void GrainBuffer::OnFileLoadProgress(float progress) {
    //
}

void GrainBuffer::OnFileLoadError(juce::String error) {
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
