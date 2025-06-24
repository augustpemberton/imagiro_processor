//
// Created by August Pemberton on 24/06/2025.
//
#include "BufferFileLoader.h"

BufferFileLoader::BufferFileLoader(): juce::Thread("Buffer file loader") {
    afm.registerBasicFormats();
}

BufferFileLoader::~BufferFileLoader() {
    stopThread(500);
}

void BufferFileLoader::loadFileIntoBuffer(const juce::File &fileToLoad, bool normalizeFile) {
    this->fileToLoad = fileToLoad;
    this->normalizeFile = normalizeFile;

    startThread();
}

void BufferFileLoader::run() {
    auto buffer = std::make_shared<juce::AudioSampleBuffer>();

    DBG("loading file " + fileToLoad.getFullPathName());
    auto reader = std::unique_ptr<juce::AudioFormatReader>(afm.createReaderFor(fileToLoad));

    if (!reader) {
        listeners.call(&Listener::OnFileLoadError, "Unable to create reader");
        return;
    }

    float magnitude = 0;

    double targetSampleRate = reader->sampleRate * oversampleRatio;

    double ratio = oversampleRatio;
    int nInterpSamples = (int)(ratio * (int)reader->lengthInSamples);

    buffer->clear();
    buffer->setSize((int)reader->numChannels, nInterpSamples);

    // for (auto c=0; c<buffer->getNumChannels(); c++) {
    //     for (auto s=0; s<buffer->getNumSamples(); s++) {
    //         buffer->setSample(c,s, 1);
    //     }
    // }
    //
    // listeners.call(&Listener::OnFileLoadComplete, buffer, reader->sampleRate, normalizeFile ? 1.f : magnitude);
    // return;


    juce::AudioSampleBuffer tempLoadBuffer ( reader->numChannels, reader->lengthInSamples);
    juce::AudioSampleBuffer tempInterpBuffer ( reader->numChannels, nInterpSamples);

    std::vector<juce::LinearInterpolator> interpolator;
    interpolator.resize(std::min((int)reader->numChannels, buffer->getNumChannels()));

    reader->read(tempLoadBuffer.getArrayOfWritePointers(), tempLoadBuffer.getNumChannels(),
                 0, reader->lengthInSamples);

    const int chunkSize = reader->lengthInSamples;
    int outN = 0;

    auto samplesToRead = (int)reader->lengthInSamples;


    for (auto n=0; n<samplesToRead; n+=std::min(samplesToRead - n, chunkSize)) {
        if (threadShouldExit()) {
            listeners.call(&Listener::OnFileLoadError, "Exit requested");
            return;
        }
        auto size = std::min(samplesToRead - n, chunkSize);
        int numInputSamples = std::min(size, tempLoadBuffer.getNumSamples() - n);
        int numOutputSamples = (int)(numInputSamples * ratio);

        // interpolate
        for (auto c = 0; c < std::min(buffer->getNumChannels(), (int) reader->numChannels); c++) {
            interpolator[c].process(1 / ratio, tempLoadBuffer.getReadPointer(c, n),
                                    tempInterpBuffer.getWritePointer(c, outN),
                                    numOutputSamples);
        }

        // load into buffer
        {
            for (auto c = 0; c < std::min(buffer->getNumChannels(), (int) reader->numChannels); c++) {
                buffer->copyFrom(c, outN, tempInterpBuffer.getReadPointer(c, outN),
                                 numOutputSamples);
            }
            magnitude = std::max(magnitude, tempInterpBuffer.getMagnitude(outN, numOutputSamples));
        }

        outN += numOutputSamples;

        listeners.call(&Listener::OnFileLoadProgress, outN / (float) nInterpSamples);
    }

    if (normalizeFile) {
        buffer->applyGain(1.f / magnitude);
    }

    listeners.call(&Listener::OnFileLoadComplete, buffer, targetSampleRate, normalizeFile ? 1.f : magnitude);
}
