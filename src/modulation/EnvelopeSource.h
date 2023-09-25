#include <utility>

//
// Created by August Pemberton on 25/09/2022.
//

#pragma once

class EnvelopeSource {
public:
    EnvelopeSource(juce::String name, int channels) : name(std::move(name))
    {
        s.ensureStorageAllocated(channels);
    }

    struct Listener {
        virtual void envelopeUpdated(EnvelopeSource* source, juce::Array<float>& sample) {}
    };

    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

    void update(juce::AudioSampleBuffer &b, int start, int end) {
        for (auto i=start; i<end; i++) {
            for (auto c=0; c<b.getNumChannels(); c++) {
                s.add(b.getSample(c, i));
            }

            listeners.call(&Listener::envelopeUpdated, this, s);
            s.clearQuick();
        }
    }

    void update(juce::AudioSampleBuffer& b, int sampleIndex) {
        for (auto c=0; c<b.getNumChannels(); c++) {
            s.add(b.getSample(c, sampleIndex));
        }

        listeners.call(&Listener::envelopeUpdated, this, s);
        s.clearQuick();
    }

    juce::String getName() { return name; }

private:
    juce::ListenerList<Listener> listeners;
    juce::String name;
    juce::Array<float> s;
};


