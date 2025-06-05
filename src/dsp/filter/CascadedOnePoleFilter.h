#pragma once

#include <juce_core/juce_core.h>
#include "imagiro_util/src/fastapprox.h"

template<int NumStages = 4>
class CascadedOnePoleFilter {
public:
    explicit CascadedOnePoleFilter(const double sample_rate = 48000, const int channels = 1)
       : cutoff(0.0), current_freq(1000.0), num_channels(channels) {
       sr_recip = 1.0 / sample_rate;

       // Initialize all stages
       for (int stage = 0; stage < NumStages; ++stage) {
           y1[stage].resize(channels, 0.0);
       }
   }

   void setChannels(const int channels) {
       num_channels = channels;
       for (int stage = 0; stage < NumStages; ++stage) {
           y1[stage].resize(channels, 0.0);
       }
   }

   void setSampleRate(const double sample_rate) {
       sr_recip = 1.0 / sample_rate;
       cutoff = 1.0 - fastexp(-6.28318530718 * current_freq * sr_recip);
   }

   void setCutoff(const double freq_hz) {
       current_freq = freq_hz;
       cutoff = 1.0 - fastexp(-6.28318530718 * freq_hz * sr_recip);
   }

   // Process one sample (lowpass) - now -6*NumStages dB/octave
   double processLP(double input, int channel = 0) {
       jassert (channel >= 0 && channel < num_channels);

       double x = input;

       // Cascade through all stages
       for (int stage = 0; stage < NumStages; ++stage) {
           y1[stage][channel] += cutoff * (x - y1[stage][channel]);
           x = y1[stage][channel];
       }

       return x;
   }

   // Process one sample (highpass) - now -6*NumStages dB/octave
   double processHP(double input, int channel = 0) {
       jassert (channel >= 0 && channel < num_channels);

       double x = input;

       // Cascade through all stages
       for (int stage = 0; stage < NumStages; ++stage) {
           y1[stage][channel] += cutoff * (x - y1[stage][channel]);
           x = y1[stage][channel];
       }

       return input - x;  // Original input minus LP output = HP
   }

   void reset() {
       for (int stage = 0; stage < NumStages; ++stage) {
           std::ranges::fill(y1[stage], 0.0);
       }
   }

   void reset(const int channel) {
       if (channel >= 0 && channel < num_channels) {
           for (int stage = 0; stage < NumStages; ++stage) {
               y1[stage][channel] = 0.0;
           }
       }
   }

   void setCutoffCoeff(const double coeff) {
       cutoff = coeff;
   }

   int getChannels() const {
       return num_channels;
   }

private:
    std::vector<double> y1[NumStages];  // Previous outputs for each stage
    double cutoff;
    double sr_recip;
    double current_freq;
    int num_channels;
};

// Convenient typedefs for common configurations
using TwoPoleFilter = CascadedOnePoleFilter<2>;    // -12dB/octave
using FourPoleFilter = CascadedOnePoleFilter<4>;   // -24dB/octave
using SixPoleFilter = CascadedOnePoleFilter<6>;    // -36dB/octave