#pragma once

#include <juce_core/juce_core.h>
#include "imagiro_util/src/fastapprox.h"

template<int NumStages = 4>
class CascadedOnePoleFilter {
public:
    explicit CascadedOnePoleFilter(const double sample_rate = 48000, const int channels = 1)
       : cutoff(0.0), target_cutoff(0.0), current_freq(1000.0), num_channels(channels) {
       sr_recip = 1.0 / sample_rate;

       // Default smoothing time of 10ms
       setSmoothingTime(0.01);

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
       updateSmoothingCoeff();

       // Update both current and target cutoff
       const double new_cutoff = 1.0 - fastexp(-6.28318530718 * current_freq * sr_recip);
       cutoff = new_cutoff;
       target_cutoff = new_cutoff;
   }

   void setCutoff(const double freq_hz, bool skipSmoothing = false) {
       current_freq = freq_hz;
       target_cutoff = 1.0 - fastexp(-6.28318530718 * freq_hz * sr_recip);
        if (skipSmoothing) cutoff = target_cutoff;
   }

   void setSmoothingTime(const double time_seconds) {
       smoothing_time = time_seconds;
       updateSmoothingCoeff();
   }

   // Process one sample (lowpass) - now -6*NumStages dB/octave
   float processLP(float input, int channel = 0) {
       jassert (channel >= 0 && channel < num_channels);

       // Smooth the cutoff coefficient
       cutoff += smoothing_coeff * (target_cutoff - cutoff);

       auto x = input;

       // Cascade through all stages
       for (int stage = 0; stage < NumStages; ++stage) {
           y1[stage][channel] += cutoff * (x - y1[stage][channel]);
           x = y1[stage][channel];
       }

       return x;
   }

   // Process one sample (highpass) - now -6*NumStages dB/octave
   float processHP(float input, int channel = 0) {
       jassert (channel >= 0 && channel < num_channels);

       // Smooth the cutoff coefficient
       cutoff += smoothing_coeff * (target_cutoff - cutoff);

       auto x = input;

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
       // Reset smoothing but keep target
       cutoff = target_cutoff;
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
       target_cutoff = coeff;
   }

   // Instantly snap to target (bypass smoothing)
   void snapToTarget() {
       cutoff = target_cutoff;
   }

   int getChannels() const {
       return num_channels;
   }

   double getCurrentCutoff() const {
       return cutoff;
   }

   double getTargetCutoff() const {
       return target_cutoff;
   }

private:
    void updateSmoothingCoeff() {
        if (sr_recip > 0.0 && smoothing_time > 0.0) {
            // Exponential smoothing coefficient
            smoothing_coeff = 1.0 - fastexp(-sr_recip / smoothing_time);
        } else {
            smoothing_coeff = 1.0; // No smoothing
        }
    }

    std::vector<double> y1[NumStages];  // Previous outputs for each stage
    double cutoff;
    double target_cutoff;
    double sr_recip;
    double current_freq;
    double smoothing_time;
    double smoothing_coeff;
    int num_channels;
};