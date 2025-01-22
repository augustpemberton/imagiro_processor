//
// Created by August Pemberton on 21/01/2025.
//

#pragma once


class BufferFileLoader : public juce::Thread {
public:

    BufferFileLoader() : juce::Thread("Buffer file loader") {
        afm.registerBasicFormats();
    }

    ~BufferFileLoader() {
        stopThread(500);
    }

    /*
     * Loads a file into the given buffer on a background thread.
     * Note - this will resize the given buffer to match the file.
     */
    void loadFileIntoBuffer(juce::File fileToLoad, juce::AudioSampleBuffer& buffer, double targetSampleRate) {
        this->fileToLoad = fileToLoad;
        this->buffer = &buffer;
        this->targetSampleRate = targetSampleRate;

        startThread();
    }

    struct Listener {
        virtual void OnFileLoadProgress(float progress) {}
        virtual void OnFileLoadComplete() {}
        virtual void OnFileLoadError(juce::String error) {}
    };

    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

private:
    juce::AudioFormatManager afm;
    juce::ListenerList<Listener> listeners;

    juce::File fileToLoad;
    juce::AudioSampleBuffer* buffer;
    double targetSampleRate;

    void run() override {
        DBG("loading file " + fileToLoad.getFullPathName());
        auto reader = std::unique_ptr<juce::AudioFormatReader>(afm.createReaderFor(fileToLoad));

        if (!reader) {
            listeners.call(&Listener::OnFileLoadError, "Unable to create reader");
            return;
        }

        auto ratio = targetSampleRate / reader->sampleRate;
        int nInterpSamples = (int)(ratio * (int)reader->lengthInSamples);

        buffer->setSize((int)reader->numChannels, nInterpSamples);

        juce::AudioSampleBuffer tempLoadBuffer ( reader->numChannels, reader->lengthInSamples);
        juce::AudioSampleBuffer tempInterpBuffer ( reader->numChannels, nInterpSamples);

        std::vector<juce::LinearInterpolator> interpolator;
        interpolator.resize(std::min((int)reader->numChannels, buffer->getNumChannels()));

        reader->read(tempLoadBuffer.getArrayOfWritePointers(), tempLoadBuffer.getNumChannels(),
                     0, reader->lengthInSamples);

        // fractional ratios give clicks in the interpolator
        // so dirty hack it and set the chunk size to the sample rate
        // this way the input and output chunk sizes should both be int
        const int chunkSize = reader->sampleRate;
        int outN = 0;

        auto samplesToRead = std::min(tempLoadBuffer.getNumSamples(), (int)(buffer->getNumSamples() * ratio));

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
            }

            outN += numOutputSamples;

            listeners.call(&Listener::OnFileLoadProgress, outN / (float) nInterpSamples);
        }

        listeners.call(&Listener::OnFileLoadComplete);
    }

};
