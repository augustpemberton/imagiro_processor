#pragma once
#include <juce_events/juce_events.h>
#include <juce_data_structures/juce_data_structures.h>

#define PORT 8081

class IPCPresetSynchroniser : juce::Timer, public juce::InterprocessConnection, public juce::InterprocessConnectionServer, juce::ValueTree::Listener {
public:
    explicit IPCPresetSynchroniser(juce::ValueTree& presetTree, bool isSender = false) : preset(presetTree), isSender(isSender) {
        if (isSender) {
            beginWaitingForSocket(PORT, "127.0.0.1");
            DBG("Listening on " + juce::String(PORT));
            preset.addListener(this);
        } else {
            if (!attemptConnection()) {
                startTimer(2000);
            }
        }
    }

    ~IPCPresetSynchroniser() override {
        disconnect();
        if (isSender)
            preset.removeListener(this);
    }

    juce::InterprocessConnection * createConnectionObject () override {
        return new IPCPresetSynchroniser(preset, isSender);
    }

    bool attemptConnection() {
        if (connectToSocket("127.0.0.1", PORT, 500)) {
            stopTimer();
            return true;
        } else return false;
    }

    void timerCallback() override {
        attemptConnection();
    }

    void connectionMade() override {
        DBG("(" + juce::String(isSender ? "sender" : "receiver") + ")" + "Connection made");
        if (isSender) sendPreset();
    }
    void connectionLost() override {
        DBG("(" + juce::String(isSender ? "sender" : "receiver") + ")" + "Connection lost");
        if (!isSender) startTimer(2000);
    }
    void messageReceived(const juce::MemoryBlock& message) override {
        juce::MemoryInputStream mis (message, false);
        auto presetString = mis.readString();
        preset = juce::ValueTree::fromXml(presetString);
    }

    void sendPreset() {
        if (!isConnected()) return;
        auto presetString = preset.toXmlString();
        juce::MemoryBlock message(presetString.toUTF8(), presetString.getNumBytesAsUTF8());
        sendMessage(message);
    }

    void valueTreePropertyChanged (juce::ValueTree &treeWhosePropertyHasChanged, const juce::Identifier &property) {
        sendPreset();
    }

    void valueTreeChildAdded (juce::ValueTree &parentTree, juce::ValueTree &childWhichHasBeenAdded) {
        sendPreset();
    }

    void valueTreeRedirected (juce::ValueTree &tree) {
        sendPreset();
    }

private:
    juce::ValueTree& preset;
    bool isSender;
};