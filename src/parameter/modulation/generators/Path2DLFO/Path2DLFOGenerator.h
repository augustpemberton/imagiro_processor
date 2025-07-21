//
// Created by August Pemberton on 21/02/2025.
//

#pragma once

#include <memory>

#include "imagiro_processor/src/curve/2d/Path2D.h"
#include "imagiro_processor/src/dsp/LookupTableLFO.h"
#include "imagiro_processor/src/parameter/modulation/ModMatrix.h"
#include "imagiro_processor/src/parameter/modulation/ModSource.h"
#include "imagiro_processor/src/parameter/Parameter.h"
#include <imagiro_processor/imagiro_processor.h>

#include "imagiro_processor/src/synth/MPESynth.h"

using namespace imagiro;

class Path2DLFO : public Parameter::Listener, MPESynth::Listener {
public:
    enum class TimingMode {
        ArcLength = 0,      // Constant visual speed
        SegmentTime = 1     // Equal time per segment
    };

    explicit Path2DLFO(MPESynth& synth, ModMatrix& modMatrix, Processor& processor, const std::string& baseName = "Path2D LFO")
        : sourceX(baseName + " X", &modMatrix, ModMatrix::SourceType::Misc, true),
          sourceY(baseName + " Y", &modMatrix, ModMatrix::SourceType::Misc, true),
          synth(synth)
    {
        synth.addListener(this);
        // Get parameter references from processor
        frequency = processor.getParameter("pathFrequency");
        phase = processor.getParameter("pathPhase");
        depth = processor.getParameter("pathDepth");
        depthX = processor.getParameter("pathDepthX");
        depthY = processor.getParameter("pathDepthY");
        bipolar = processor.getParameter("pathBipolar");
        timingModeParam = processor.getParameter("pathTimingMode");
        playbackMode = processor.getParameter("pathPlaybackMode");
        mono = processor.getParameter("pathMono");
        syncToHost = processor.getParameter("pathSyncToHost");

        // Add listeners
        frequency->addListener(this);
        phase->addListener(this);
        bipolar->addListener(this);
        timingModeParam->addListener(this);
        playbackMode->addListener(this);
        mono->addListener(this);

        setFrequency(frequency->getProcessorValue());
        sourceX.setBipolar(bipolar->getBoolValue());
        sourceY.setBipolar(bipolar->getBoolValue());

        // Set default path (triangle-like shape)
        Path2D defaultPath;
        defaultPath.addSegment(BezierSegment(
            Point2D(0.0f, 0.0f),    // start
            Point2D(0.2f, 0.8f),    // control1
            Point2D(0.3f, 0.9f),    // control2
            Point2D(0.5f, 1.0f)     // end
        ));
        defaultPath.addSegment(BezierSegment(
            Point2D(0.5f, 1.0f),    // start
            Point2D(0.7f, 0.9f),    // control1
            Point2D(0.8f, 0.2f),    // control2
            Point2D(1.0f, 0.0f)     // end
        ));
        defaultPath.addSegment(BezierSegment(
            Point2D(1.f, 0.0f),    // start
            Point2D(0.7f, 0.9f),    // control1
            Point2D(0.8f, 0.2f),    // control2
            Point2D(0.5f, 0.9f)     // end
        ));

        setPath(defaultPath);
    }

    ~Path2DLFO() override {
        frequency->removeListener(this);
        phase->removeListener(this);
        bipolar->removeListener(this);
        timingModeParam->removeListener(this);
        playbackMode->removeListener(this);
        mono->removeListener(this);
        synth.removeListener(this);
    }

    void prepareToPlay(const double sampleRate, int maximumExpectedSamplesPerBlock) {
        xLFO.setSampleRate(sampleRate);
        yLFO.setSampleRate(sampleRate);

        for (auto& voiceLFO : voiceXLFOs) {
            voiceLFO.setSampleRate(sampleRate);
        }
        for (auto& voiceLFO : voiceYLFOs) {
            voiceLFO.setSampleRate(sampleRate);
        }
    }

    void setPath(const Path2D& newPath) {
        path = newPath;
        regenerateLookupTables();
    }

    const Path2D& getPath() const {
        return path;
    }

    // Main process function called from processor
    void process(const int numSamples = 1) {
        if (mono->getBoolValue()) {
            processGlobal(numSamples);
        } else {
            // Process all active voices
            for (const auto& voiceIndex : activeVoices) {
                processVoice(voiceIndex, numSamples);
            }
        }
    }

    // Global (mono) processing
    void processGlobal(const int numSamples = 1) {
        if (!mono->getBoolValue()) return;

        // Process both X and Y LFOs
        const float xValue = xLFO.process(numSamples);
        const float yValue = yLFO.process(numSamples);

        // Apply scaling and set sources
        const float depthScale = depth->getProcessorValue();
        const float depthScaleX = depthX->getProcessorValue();
        const float depthScaleY = depthY->getProcessorValue();

        sourceX.setGlobalValue(xValue * depthScale * depthScaleX * 2 - 1);
        sourceY.setGlobalValue(yValue * depthScale * depthScaleY * 2 - 1);
    }

    // Per-voice processing
    void processVoice(const size_t voiceIndex, const int numSamples = 1) {
        if (mono->getBoolValue()) return;

        // Process both X and Y LFOs for this voice
        const float xValue = voiceXLFOs[voiceIndex].process(numSamples);
        const float yValue = voiceYLFOs[voiceIndex].process(numSamples);

        // Apply scaling and set sources
        const float depthScale = depth->getProcessorValue(voiceIndex);
        const float depthScaleX = depthX->getProcessorValue(voiceIndex);
        const float depthScaleY = depthY->getProcessorValue(voiceIndex);

        sourceX.setVoiceValue(xValue * depthScale * depthScaleX * 2 - 1, voiceIndex);
        sourceY.setVoiceValue(yValue * depthScale * depthScaleY * 2 - 1, voiceIndex);
    }

    void onVoiceStarted(const size_t voiceIndex) override {
        activeVoices.insert(voiceIndex);

        // Initialize voice LFOs if needed
        if (!voiceInitialized[voiceIndex]) {
            voiceXLFOs[voiceIndex].setFrequency(xLFO.getFrequency());
            voiceXLFOs[voiceIndex].setPhaseOffset(xLFO.getPhaseOffset());
            voiceYLFOs[voiceIndex].setFrequency(yLFO.getFrequency());
            voiceYLFOs[voiceIndex].setPhaseOffset(yLFO.getPhaseOffset());

            // Set the lookup tables
            if (xTable && yTable) {
                voiceXLFOs[voiceIndex].setTable(xTable);
                voiceYLFOs[voiceIndex].setTable(yTable);
            }

            voiceInitialized[voiceIndex] = true;
        }

        // Retrigger if needed
        if (playbackMode->getProcessorValue() != 1) {
            voiceXLFOs[voiceIndex].setPhase(0.0f);
            voiceYLFOs[voiceIndex].setPhase(0.0f);
        }
    }

    void onVoiceFinished(const size_t voiceIndex) override {
        activeVoices.erase(voiceIndex);

        // Clear voice values
        sourceX.setVoiceValue(0.0f, voiceIndex);
        sourceY.setVoiceValue(0.0f, voiceIndex);
    }

    // Get current phase for display purposes
    float getCurrentPhase() const {
        if (mono->getBoolValue()) return xLFO.getPhase();

        // Return phase of most recently triggered voice or 0
        if (!activeVoices.empty()) {
            return voiceXLFOs[*activeVoices.rbegin()].getPhase();
        }

        return 0.0f;
    }

    // Get current position on path for display
    Point2D getCurrentPosition() const {
        const float phaseValue = getCurrentPhase();
        return timingMode == TimingMode::ArcLength ?
               path.getPointAt(phaseValue) :
               path.getPointAtSegmentTime(phaseValue);
    }

    ModSource& getSourceX() { return sourceX; }
    ModSource& getSourceY() { return sourceY; }

    const ModSource& getSourceX() const { return sourceX; }
    const ModSource& getSourceY() const { return sourceY; }

    // Parameter::Listener implementation
    void parameterChangedSync(Parameter* p) override {
        if (p == frequency) {
            setFrequency(p->getProcessorValue());
        } else if (p == phase) {
            setPhaseOffset(p->getProcessorValue());
        } else if (p == bipolar) {
            sourceX.setBipolar(p->getBoolValue());
            sourceY.setBipolar(p->getBoolValue());
        } else if (p == timingModeParam) {
            const auto mode = static_cast<TimingMode>(static_cast<int>(p->getProcessorValue()));
            setTimingMode(mode);
        } else if (p == playbackMode) {
            const bool envMode = p->getProcessorValue() == 2;
            xLFO.setStopAfterOneOscillation(envMode);
            yLFO.setStopAfterOneOscillation(envMode);
            for (auto& voiceLFO : voiceXLFOs) {
                voiceLFO.setStopAfterOneOscillation(envMode);
            }
            for (auto& voiceLFO : voiceYLFOs) {
                voiceLFO.setStopAfterOneOscillation(envMode);
            }
        } else if (p == mono) {
            if (!p->getBoolValue()) {
                // Clear global values when switching to poly
                sourceX.setGlobalValue(0.0f);
                sourceY.setGlobalValue(0.0f);
            } else {
                // Clear all voice values when switching to mono
                for (size_t i = 0; i < MAX_MOD_VOICES; ++i) {
                    sourceX.setVoiceValue(0.0f, i);
                    sourceY.setVoiceValue(0.0f, i);
                }
            }
        }
    }

private:
    Path2D path;
    TimingMode timingMode = TimingMode::ArcLength;

    ModSource sourceX;
    ModSource sourceY;

    // X and Y LFOs with lookup tables
    LookupTableLFO xLFO, yLFO;
    LookupTableLFO voiceXLFOs[MAX_MOD_VOICES];
    LookupTableLFO voiceYLFOs[MAX_MOD_VOICES];

    // Shared lookup tables
    std::shared_ptr<juce::dsp::LookupTable<float>> xTable;
    std::shared_ptr<juce::dsp::LookupTable<float>> yTable;

    // Voice management
    std::set<size_t> activeVoices;
    bool voiceInitialized[MAX_MOD_VOICES] = {false};

    // Parameter references
    Parameter* frequency;
    Parameter* phase;
    Parameter* depth;
    Parameter* depthX;
    Parameter* depthY;
    Parameter* bipolar;
    Parameter* timingModeParam;
    Parameter* playbackMode;
    Parameter* mono;
    Parameter* syncToHost;

    MPESynth& synth;

    // Internal setter methods (not exposed publicly)
    void setFrequency(const float freq) {
        xLFO.setFrequency(freq);
        yLFO.setFrequency(freq);

        for (auto& voiceLFO : voiceXLFOs) {
            voiceLFO.setFrequency(freq);
        }
        for (auto& voiceLFO : voiceYLFOs) {
            voiceLFO.setFrequency(freq);
        }
    }

    void setPhaseOffset(const float offset) {
        xLFO.setPhaseOffset(offset);
        yLFO.setPhaseOffset(offset);

        for (auto& voiceLFO : voiceXLFOs) {
            voiceLFO.setPhaseOffset(offset);
        }
        for (auto& voiceLFO : voiceYLFOs) {
            voiceLFO.setPhaseOffset(offset);
        }
    }

    void setTimingMode(const TimingMode mode) {
        if (timingMode != mode) {
            timingMode = mode;
            regenerateLookupTables();
        }
    }

    void regenerateLookupTables() {
        const bool useArcLength = (timingMode == TimingMode::ArcLength);
        auto tables = path.generateLookupTables(useArcLength);
        xTable = tables.first;
        yTable = tables.second;

        // Update global LFOs
        xLFO.setTable(xTable);
        yLFO.setTable(yTable);

        // Update voice LFOs
        for (auto& voiceLFO : voiceXLFOs) {
            voiceLFO.setTable(xTable);
        }
        for (auto& voiceLFO : voiceYLFOs) {
            voiceLFO.setTable(yTable);
        }
    }
};