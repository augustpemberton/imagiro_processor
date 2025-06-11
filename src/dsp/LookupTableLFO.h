#pragma once

class LookupTableLFO : juce::Timer
{
public:
    explicit LookupTableLFO(const int numPoints = 128)
        : tableSize(numPoints)
    {
        lookupTable = std::make_shared<juce::dsp::LookupTable<float>>([numPoints](const size_t n) {
            return std::sin(n / static_cast<float>(numPoints) * juce::MathConstants<float>::twoPi);
        }, tableSize);

        smoothedPhaseOffset.reset(100);
    }

    void setSampleRate(const double newSampleRate)
    {
        sampleRate = newSampleRate;
        smoothedPhaseOffset.reset(sampleRate, smoothingTimeSeconds);
        updatePhaseIncrement();
    }

    void setFrequency(const float frequencyHz)
    {
        frequency = frequencyHz;
        updatePhaseIncrement();
    }

    void setPhase(const float newPhase)
    {
        // Ensure phase is in [0, 1] range
        phase = std::fmod(newPhase, 1.0f);
        if (phase < 0.0f)
            phase += 1.0f;
    }

    void setPhaseOffset(const float newOffset)
    {
        // Ensure offset is in [0, 1] range
        const auto wrappedOffset = newOffset - static_cast<int>(newOffset);
        smoothedPhaseOffset.setTargetValue(wrappedOffset);
    }

    void setSmoothingTime(const float seconds)
    {
        smoothingTimeSeconds = seconds;
        if (sampleRate > 0) smoothedPhaseOffset.reset(sampleRate, smoothingTimeSeconds);
    }

    float process(const int numSamples = 1)
    {

        // load new table
        {
            bool newTable = false;
            std::shared_ptr<juce::dsp::LookupTable<float>> tempTable;
            while (tablesToLoad.try_dequeue(tempTable)) {newTable = true;}

            if (newTable) {
                tablesToDeallocate.enqueue(lookupTable);
                lookupTable = tempTable;
                tableSize = lookupTable->getNumPoints();
            }
        }

        // Process multiple samples if needed
        for (int i = 0; i < numSamples; ++i)
        {
            // Advance the phase
            phase += phaseIncrement;

            // Wrap phase to [0, 1]
            if (phase >= 1.0f) {
                if (stopAfterOneOscillation) phase = 1.0f;
                else phase -= 1.0f;
            }

            // Update smoothed phase offset
            currentPhaseOffset = smoothedPhaseOffset.getNextValue();
        }

        // Calculate total phase with offset
        float totalPhase = phase + currentPhaseOffset;

        // Wrap total phase to [0, 1]
        if (totalPhase >= 1.0f) totalPhase -= 1.0f;

        // Get value from lookup table
        const auto v = lookupTable->get(totalPhase * static_cast<float>(tableSize - 1));

        return v;
    }

    float getCurrentValue() const
    {
        // Get current value without advancing phase
        float totalPhase = phase + currentPhaseOffset;

        if (totalPhase >= 1.0f)
            totalPhase -= 1.0f;

        return lookupTable->getUnchecked(totalPhase * (tableSize - 1));
    }

    float getPhase() const {
        const auto p = phase + currentPhaseOffset;
        return p - static_cast<int>(p);
    }
    float getFrequency() const { return frequency; }
    float getPhaseOffset() const { return smoothedPhaseOffset.getTargetValue(); }
    float getCurrentPhaseOffset() const { return currentPhaseOffset; }

    // call from message thread
    void setTable(const std::shared_ptr<juce::dsp::LookupTable<float>> &table) {
        tablesToLoad.enqueue(table);
    }

    void setStopAfterOneOscillation(bool shouldStop) {
        stopAfterOneOscillation = shouldStop;
    }

private:
    void updatePhaseIncrement()
    {
        if (sampleRate > 0)
            phaseIncrement = frequency / static_cast<float>(sampleRate);
        else
            phaseIncrement = 0.0f;
    }

    size_t tableSize;

    std::shared_ptr<juce::dsp::LookupTable<float>> lookupTable;
    moodycamel::ReaderWriterQueue<std::shared_ptr<juce::dsp::LookupTable<float>>> tablesToLoad {4};
    moodycamel::ReaderWriterQueue<std::shared_ptr<juce::dsp::LookupTable<float>>> tablesToDeallocate {4};

    void timerCallback() override {
        std::shared_ptr<juce::dsp::LookupTable<float>> temp;
        while (tablesToDeallocate.try_dequeue(temp)) {}
    }

    juce::SmoothedValue<float> smoothedPhaseOffset;

    float phase = 0.0f;              // Current phase [0, 1]
    float frequency = 1.0f;          // Frequency in Hz
    float phaseIncrement = 0.0f;     // Phase increment per sample
    float currentPhaseOffset = 0.0f; // Current smoothed offset value
    float smoothingTimeSeconds = 0.1f; // Smoothing time for phase offset
    double sampleRate = 44100.0;

    bool stopAfterOneOscillation = false;
};