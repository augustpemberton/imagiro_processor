#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

template<int NumStages = 2>  // Typically use fewer stages since each is 2nd order
class CascadedBiquadFilter {
private:
    struct BiquadCoeffs {
        double b0, b1, b2;  // Numerator coefficients
        double a1, a2;      // Denominator coefficients (a0 = 1)
    };

    struct BiquadState {
        double x1, x2;  // Input delay line
        double y1, y2;  // Output delay line
        
        BiquadState() : x1(0.0), x2(0.0), y1(0.0), y2(0.0) {}
    };

public:
    enum FilterType {
        LOWPASS,
        HIGHPASS,
        BANDPASS,
        NOTCH,
        ALLPASS
    };

    explicit CascadedBiquadFilter(const double sample_rate = 48000, const int channels = 1)
       : current_freq(1000.0), current_q(0.707), filter_type(LOWPASS), 
         num_channels(channels), sample_rate(sample_rate) {
        
        // Initialize states for all stages and channels
        for (int stage = 0; stage < NumStages; ++stage) {
            states[stage].resize(channels);
        }
        
        // Calculate initial coefficients
        updateCoefficients();
    }

    void setChannels(const int channels) {
        num_channels = channels;
        for (int stage = 0; stage < NumStages; ++stage) {
            states[stage].resize(channels);
        }
    }

    void setSampleRate(const double sr) {
        sample_rate = sr;
        updateCoefficients();
    }

    void setCutoff(const double freq_hz) {
        current_freq = juce::jlimit(20.0, sample_rate * 0.45, freq_hz);
        updateCoefficients();
    }

    void setQ(const double q_factor) {
        current_q = juce::jlimit(0.1, 100.0, q_factor);
        updateCoefficients();
    }

    void setFilterType(FilterType type) {
        filter_type = type;
        updateCoefficients();
    }

    // Process one sample through all cascaded stages
    float process(float input, int channel = 0) {
        jassert(channel >= 0 && channel < num_channels);

        float x = input;
        
        // Process through each biquad stage
        for (int stage = 0; stage < NumStages; ++stage) {
            x = processBiquad(x, stage, channel);
        }

        return x;
    }

    void reset() {
        for (int stage = 0; stage < NumStages; ++stage) {
            for (auto& state : states[stage]) {
                state = BiquadState();
            }
        }
    }

    void reset(const int channel) {
        if (channel >= 0 && channel < num_channels) {
            for (int stage = 0; stage < NumStages; ++stage) {
                states[stage][channel] = BiquadState();
            }
        }
    }

    // Getters
    int getChannels() const { return num_channels; }
    double getCurrentFreq() const { return current_freq; }
    double getCurrentQ() const { return current_q; }
    FilterType getFilterType() const { return filter_type; }

private:
    void updateCoefficients() {
        const double omega = 6.28318530718 * current_freq / sample_rate;
        const double cos_omega = std::cos(omega);
        const double sin_omega = std::sin(omega);
        const double alpha = sin_omega / (2.0 * current_q);

        // Calculate coefficients based on filter type
        switch (filter_type) {
            case LOWPASS:
                calculateLowpassCoeffs(cos_omega, alpha);
                break;
            case HIGHPASS:
                calculateHighpassCoeffs(cos_omega, alpha);
                break;
            case BANDPASS:
                calculateBandpassCoeffs(cos_omega, alpha);
                break;
            case NOTCH:
                calculateNotchCoeffs(cos_omega, alpha);
                break;
            case ALLPASS:
                calculateAllpassCoeffs(cos_omega, alpha);
                break;
        }
    }

    void calculateLowpassCoeffs(double cos_omega, double alpha) {
        const double a0 = 1.0 + alpha;
        coeffs.b0 = (1.0 - cos_omega) / (2.0 * a0);
        coeffs.b1 = (1.0 - cos_omega) / a0;
        coeffs.b2 = coeffs.b0;
        coeffs.a1 = (-2.0 * cos_omega) / a0;
        coeffs.a2 = (1.0 - alpha) / a0;
    }

    void calculateHighpassCoeffs(double cos_omega, double alpha) {
        const double a0 = 1.0 + alpha;
        coeffs.b0 = (1.0 + cos_omega) / (2.0 * a0);
        coeffs.b1 = -(1.0 + cos_omega) / a0;
        coeffs.b2 = coeffs.b0;
        coeffs.a1 = (-2.0 * cos_omega) / a0;
        coeffs.a2 = (1.0 - alpha) / a0;
    }

    void calculateBandpassCoeffs(double cos_omega, double alpha) {
        const double a0 = 1.0 + alpha;
        coeffs.b0 = alpha / a0;
        coeffs.b1 = 0.0;
        coeffs.b2 = -alpha / a0;
        coeffs.a1 = (-2.0 * cos_omega) / a0;
        coeffs.a2 = (1.0 - alpha) / a0;
    }

    void calculateNotchCoeffs(double cos_omega, double alpha) {
        const double a0 = 1.0 + alpha;
        coeffs.b0 = 1.0 / a0;
        coeffs.b1 = (-2.0 * cos_omega) / a0;
        coeffs.b2 = 1.0 / a0;
        coeffs.a1 = coeffs.b1;
        coeffs.a2 = (1.0 - alpha) / a0;
    }

    void calculateAllpassCoeffs(double cos_omega, double alpha) {
        const double a0 = 1.0 + alpha;
        coeffs.b0 = (1.0 - alpha) / a0;
        coeffs.b1 = (-2.0 * cos_omega) / a0;
        coeffs.b2 = 1.0 / a0;
        coeffs.a1 = coeffs.b1;
        coeffs.a2 = coeffs.b0;
    }

    float processBiquad(float input, int stage, int channel) {
        auto& state = states[stage][channel];
        
        // Direct Form II Transposed implementation
        const float output = static_cast<float>(
            coeffs.b0 * input + 
            coeffs.b1 * state.x1 + 
            coeffs.b2 * state.x2 - 
            coeffs.a1 * state.y1 - 
            coeffs.a2 * state.y2
        );

        // Update delay lines
        state.x2 = state.x1;
        state.x1 = input;
        state.y2 = state.y1;
        state.y1 = output;

        return output;
    }

private:
    std::vector<BiquadState> states[NumStages];  // State for each stage and channel
    BiquadCoeffs coeffs;  // Single set of coefficients
    
    double current_freq;
    double current_q;
    FilterType filter_type;
    
    double sample_rate;
    int num_channels;
};