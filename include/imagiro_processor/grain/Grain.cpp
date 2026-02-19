//
// Created by August Pemberton on 23/08/2022.
//
#include "Grain.h"

Grain::Grain(std::vector<GrainSampleData> &data, size_t i)
    : indexInStream(i), isLooping(false), sampleDataBuffer(data), windowFunction(),
      cachedPanCoeffs{}, gain(0), pointer(0),
      sampleRate(0)
{
}

// Note: this will be called while the grain is running if stream settings change, so it needs to be fast
// if something is slow to configure, just do it one time in play()
void Grain::configure(GrainSettings s) {

    // don't update reverse while playing
    if (isPlaying()) {
        s.reverse = settings.reverse;
    }

    settings = s;
    updateLoopSettings(settings.loopSettings);
    calculatePanCoeffs(settings.pan + spreadVal);
    smoothPitchRatio.setTargetValue(settings.getPitchRatio());
}

float *Grain::calculatePanCoeffs(const float val) {
    if (val != cachedPan) {
        cachedPanCoeffs[0] = sinApprox((1 - val) * juce::MathConstants<float>::pi / 2);
        cachedPanCoeffs[1] = sinApprox(val * juce::MathConstants<float>::pi / 2);
        cachedPan = val;
    }
    return cachedPanCoeffs;
}

void Grain::play(int sampleDelay) {
    if (!currentBuffer) {
        jassertfalse;
        return;
    }

    spreadVal = (imagiro::rand01() * 2 - 1) * settings.spread;

    // if the grain is infinitely long, don't use the window function, just play full volume
    if (settings.duration < 0) {
        progressPerSample = 0;
        windowFunction.initialise([](float p) {
            return 1.f;
        }, 0.f, 1.f, 2);
    } else {
        progressPerSample = 1.f / (settings.duration * (float) sampleRate);
        windowFunction.initialise([shape = settings.shape, skew = settings.skew](const float p) {
            return getGrainShapeGain(p, shape, skew);
        }, 0.f, 1.f, 100);
    }


    updateSampleRateRatio();

    auto spawnPosition = std::clamp(settings.position, 0.f, 1.f) * static_cast<float>(currentBuffer->buffer.getNumSamples());

    setNewLoopSettingsInternal(settings.loopSettings);

    if (settings.loopSettings.loopActive) {
        const auto loopStart = settings.loopSettings.getLoopStartSample(currentBuffer->buffer.getNumSamples());
        const auto loopEnd = settings.loopSettings.getLoopEndSample(currentBuffer->buffer.getNumSamples());
        bool pastLoop = false;
        if (!settings.reverse) {
            pastLoop = spawnPosition >= loopEnd;
        } else {
            pastLoop = spawnPosition <= loopStart;
        }

        if (pastLoop) spawnPosition = imagiro::wrapWithinRange(spawnPosition, loopStart, loopEnd);
    }

    pointer = spawnPosition;
    pointer = std::min(
        static_cast<double>(currentBuffer->buffer.getNumSamples()) - INTERP_POST_SAMPLES - INTERP_PRE_SAMPLES - 1 -
        quickfadeSamples, pointer);
    pointer = std::max(static_cast<double>(INTERP_PRE_SAMPLES), pointer);
    progress = 0;

    samplesUntilStart = sampleDelay;
    playing = true;

    listeners.call(&Listener::OnGrainStarted, *this);
    quickfading = false;
    quickfadeGain = 1.f;
    firstBlockFlag = true;

    isLooping = false;
    loopFadePointer = -1;
    loopFadeProgress = 0;
}

float Grain::getGrainSpeed() const {
    return settings.getPitchRatio();
}

int Grain::getSamplesUntilEndOfBuffer() const {
    auto maxPitchRatio = std::max(std::abs(smoothPitchRatio.getCurrentValue()),
                                  std::abs(smoothPitchRatio.getTargetValue()));
    maxPitchRatio *= sampleRateRatio;

    int s;
    if (settings.reverse) {
        s = (int) ((pointer - INTERP_PRE_SAMPLES) / maxPitchRatio);
    } else {
        s = (int) ((currentBuffer->buffer.getNumSamples() - INTERP_POST_SAMPLES - pointer) / (maxPitchRatio));
    }

    return s;
}

void Grain::updateLoopSettings(LoopSettings s, bool force) {
    if (settings.loopSettings == s && !force) return;

    if (isLooping) queuedLoopSettings = s;
    else setNewLoopSettingsInternal(s);
}

void Grain::resetBuffer() { currentBuffer.reset(); }

void Grain::setBuffer(const std::shared_ptr<imagiro::InfoBuffer>& buf) {
    if (buf == currentBuffer) return;
    currentBuffer = buf;
}

void Grain::processBlock(juce::AudioSampleBuffer &out, int outStartSample, int numSamples, bool setNotAdd) {
    jassert(currentBuffer); // make sure to setBuffer() first!

    if (samplesUntilStart > 0) {
        auto delay = samplesUntilStart;
        samplesUntilStart = std::max(0, samplesUntilStart - numSamples);
        if (samplesUntilStart > 0) return;

        outStartSample += delay;
        numSamples -= delay;
    }

    if (stopFlag) {
        quickfading = true;
        stopFlag = false;
    }

    const auto &bounds = cachedLoopBoundaries;
    const auto pastLoopRegion = settings.loopSettings.loopActive && pointer > bounds.loopEndSample;

    if (pastLoopRegion || !settings.loopSettings.loopActive) {
        const auto samplesUntilEnd = getSamplesUntilEndOfBuffer() - INTERP_POST_SAMPLES - 1;
        if (samplesUntilEnd < numSamples) {
            stop(false);
            return;
        }
        if (samplesUntilEnd < quickfadeSamples + numSamples) {
            quickfading = true;
            numSamples = std::min(numSamples, samplesUntilEnd - 1);
        }
    }

    // Check if we're already at zero gain
    if (quickfading && quickfadeGain <= 0.f) {
        stop(false);
        return;
    }

    if (progressPerSample > 0) {
        auto samplesUntilEndOfGrain = static_cast<int>((1 - progress) / progressPerSample);
        numSamples = std::min(numSamples, samplesUntilEndOfGrain);
    }

    if (firstBlockFlag) {
        firstBlockFlag = false;
        smoothPitchRatio.setCurrentAndTargetValue(smoothPitchRatio.getTargetValue());
    }

    // Cache frequently accessed values
    const auto numBufferChannels = currentBuffer->buffer.getNumChannels();
    const auto numOutChannels = out.getNumChannels();
    const bool isLoopActive = settings.loopSettings.loopActive;
    const bool isReverse = settings.reverse;


    while (numSamples > 0 && quickfadeGain > 0.f) {
        auto samplesThisChunk = numSamples;

        auto startPitchRatio = smoothPitchRatio.getCurrentValue() * sampleRateRatio;
        smoothPitchRatio.skip(samplesThisChunk);
        auto endPitchRatio = smoothPitchRatio.getCurrentValue() * sampleRateRatio;
        auto pitchRatioPerSample = (endPitchRatio - startPitchRatio) / static_cast<float>(samplesThisChunk);

        auto startPan = smoothedPan.getCurrentValue();
        smoothedPan.skip(samplesThisChunk);
        auto endPan = smoothedPan.getCurrentValue();
        auto panPerSample = (endPan - startPan) / static_cast<float>(samplesThisChunk);

        auto quickfadeGainStart = quickfadeGain;
        int maxQuickfadeSamples = quickfading
                                      ? static_cast<int>(quickfadeGainStart / quickfadeGainPerSample) + 1
                                      : samplesThisChunk;
        samplesThisChunk = std::min(samplesThisChunk, maxQuickfadeSamples);

        // Ensure our buffer is large enough
        if (sampleDataBuffer.size() < static_cast<size_t>(samplesThisChunk)) {
            sampleDataBuffer.resize(samplesThisChunk);
        }

        // Pre-calculate all position and loop data
        double pos = pointer;
        bool looping = isLooping;
        for (int s = 0; s < samplesThisChunk; s++) {
            auto pitchRatio = startPitchRatio + static_cast<float>(s) * pitchRatioPerSample;
            pos += pitchRatio;

            // Handle loop wrapping
            if (isLoopActive) {
                if (!isReverse && pos >= bounds.loopEndSample) {
                    pos -= bounds.loopLengthSamples;
                    pos += bounds.loopCrossfadeSamples;
                    looping = false;
                } else if (isReverse && pos <= bounds.loopStartSample) {
                    pos += bounds.loopLengthSamples;
                    pos -= bounds.loopCrossfadeSamples;
                    looping = false;
                }

                // Check for loop fade start
                if (!isReverse && pos - pitchRatio < bounds.loopFadeStart && pos >= bounds.loopFadeStart) {
                    looping = true;
                } else if (isReverse && pos - pitchRatio > bounds.loopFadeStartReverse && pos <= bounds.
                           loopFadeStartReverse) {
                    looping = true;
                }
            }

            sampleDataBuffer[s].position = pos;
            sampleDataBuffer[s].looping = looping;


            // Pre-calculate loop fade data if needed
            if (looping && bounds.loopCrossfadeSamples > 0) {
                if (!isReverse && pos >= bounds.loopFadeStart && pos <= bounds.loopEndSample + 1) {
                    const auto distanceIntoFade = pos - bounds.loopFadeStart;
                    sampleDataBuffer[s].loopFadePointer = bounds.loopStartSample + distanceIntoFade;
                    sampleDataBuffer[s].loopFadeProgress = distanceIntoFade / bounds.loopCrossfadeSamples;
                } else if (isReverse && pos >= bounds.loopStartSample - 1 && pos <= bounds.loopFadeStartReverse) {
                    const auto distanceIntoFade = bounds.loopFadeStartReverse - pos;
                    sampleDataBuffer[s].loopFadePointer = bounds.loopEndSample - distanceIntoFade;
                    sampleDataBuffer[s].loopFadeProgress = distanceIntoFade / bounds.loopCrossfadeSamples;
                } else {
                    sampleDataBuffer[s].loopFadePointer = -1;
                    sampleDataBuffer[s].loopFadeProgress = 0;
                }
            } else {
                sampleDataBuffer[s].loopFadePointer = -1;
                sampleDataBuffer[s].loopFadeProgress = 0;
            }
        }

        // Now process all channels using pre-calculated data
        for (int c = 0; c < numOutChannels; c++) {
            auto inChannel = c % numBufferChannels;
            auto stereoOutChannel = c % 2;
            const auto* bufferPointer = currentBuffer->buffer.getReadPointer(inChannel);

            for (int s = 0; s < samplesThisChunk; s++) {
                const auto &sample = sampleDataBuffer[s];

                auto v = interp4p3o_2x(bufferPointer, sample.position);

                // Apply loop crossfade if needed
                if (sample.loopFadePointer >= 0) {
                    const auto fadeSample = interp4p3o_2x(bufferPointer,
                                                                   static_cast<float>(sample.loopFadePointer));
                    v = v * (1 - sample.loopFadeProgress) + fadeSample * sample.loopFadeProgress;
                }

                // Apply window function
                v *= windowFunction.processSampleUnchecked(progress + static_cast<float>(s) * progressPerSample);
                v *= settings.gain;

                // Apply quickfade
                if (quickfading) {
                    const auto quickfadeAmount = quickfadeGainStart - quickfadeGainPerSample * static_cast<float>(s);
                    if (quickfadeAmount <= 0.f) break;
                    v *= quickfadeAmount;
                }

                // Apply panning
                const auto pan = startPan + static_cast<float>(s) * panPerSample;
                v *= numBufferChannels > 1 ? calculatePanCoeffs(pan + spreadVal)[stereoOutChannel] : 1.f;

                if (setNotAdd) out.setSample(c, outStartSample + s, v);
                else out.addSample(c, outStartSample + s, v);
            }
        }

        // Update grain state with final values
        pointer = sampleDataBuffer[samplesThisChunk - 1].position;
        isLooping = sampleDataBuffer[samplesThisChunk - 1].looping;
        if (isLooping) {
            loopFadePointer = sampleDataBuffer[samplesThisChunk - 1].loopFadePointer;
            loopFadeProgress = sampleDataBuffer[samplesThisChunk - 1].loopFadeProgress;
        }

        progress += progressPerSample * static_cast<float>(samplesThisChunk);
        if (quickfading) quickfadeGain -= quickfadeGainPerSample * static_cast<float>(samplesThisChunk);

        numSamples -= samplesThisChunk;
    }

    if (!isLooping && queuedLoopSettings.has_value()) {
        setNewLoopSettingsInternal(*queuedLoopSettings);
        queuedLoopSettings.reset();
    }

    const auto wasPlaying = playing;
    playing = progress + progressPerSample < 1;

    if (quickfadeGain <= 0.f) {
        stop(false);
    }

    if (!playing && wasPlaying) {
        stop(false);
    }
}


void Grain::updateSampleRateRatio() {
    if (!currentBuffer) return;
    sampleRateRatio = currentBuffer->sampleRate / sampleRate;
}

void Grain::setNewLoopSettingsInternal(const LoopSettings loopSettings) {
    settings.loopSettings = loopSettings;

    // check if we've just gone over the current position
    const auto bufferLength = currentBuffer->buffer.getNumSamples();
    const auto loopStartSample = settings.loopSettings.getLoopStartSample(bufferLength);
    const auto loopCrossfadeSamples = settings.loopSettings.getCrossfadeSamples(bufferLength);
    const auto loopEndSample = settings.loopSettings.getLoopEndSample(bufferLength);
    const auto loopLengthSamples = settings.loopSettings.getLoopLengthSamples(bufferLength);

    auto newLengthSamples = loopLengthSamples;
    auto newLoopCrossfadeSamples = loopCrossfadeSamples;

    // adding a little padding to compensate for float rounding
    const auto nextPointerPos = pointer + getCurrentPitchRatio();
    constexpr auto bufferStart = INTERP_PRE_SAMPLES;
    const auto bufferEnd = currentBuffer->buffer.getNumSamples() - 1 - INTERP_POST_SAMPLES;

    if (!settings.reverse) {
        auto minLoopEnd = std::min(bufferEnd, static_cast<int>(nextPointerPos + loopCrossfadeSamples));
        minLoopEnd += 1; // compensate for rounding errors
        if (loopEndSample < minLoopEnd) {
            newLoopCrossfadeSamples = std::min(newLoopCrossfadeSamples,
                                               minLoopEnd - static_cast<int>(nextPointerPos + 1));
            newLengthSamples = minLoopEnd - loopStartSample;
            const auto newLengthPercentage = newLengthSamples / static_cast<float>(bufferLength);
            settings.loopSettings.loopLength = newLengthPercentage;
        }
    } else {
        auto maxLoopStart = std::max(bufferStart, static_cast<int>(nextPointerPos - loopCrossfadeSamples));
        maxLoopStart -= 1; // compensate for rounding errors
        if (loopStartSample > maxLoopStart) {
            newLoopCrossfadeSamples = std::min(newLoopCrossfadeSamples,
                                               static_cast<int>(nextPointerPos) - maxLoopStart);
            newLengthSamples = loopEndSample - maxLoopStart;

            const auto newStartPercentage = maxLoopStart / static_cast<float>(bufferLength);
            settings.loopSettings.loopStart = newStartPercentage;

            const auto newLengthPercentage = newLengthSamples / static_cast<float>(bufferLength);
            settings.loopSettings.loopLength = newLengthPercentage;
        }
    }


    if (newLengthSamples != loopLengthSamples || newLoopCrossfadeSamples != loopCrossfadeSamples) {
        // update crossfade percentage to keep the same number of samples
        const auto newCrossfadePercentage = newLoopCrossfadeSamples / static_cast<float>(newLengthSamples) * 2;
        settings.loopSettings.loopCrossfade = newCrossfadePercentage;
    }

    updateCachedLoopBoundaries();
}


void Grain::stop(bool fadeout) {
    if (fadeout) stopFlag = true;
    else {
        playing = false;
        listeners.call(&Listener::OnGrainFinished, *this);
    }
}

void Grain::prepareToPlay(double sr, int maxBlockSize) {
    this->sampleRate = sr;
    updateSampleRateRatio();

    smoothPitchRatio.reset(sr, 0.01);
    smoothedPan.reset(sr, 0.01);

    quickfadeSamples = static_cast<int>(quickfadeSeconds * (float) sr);
    quickfadeGainPerSample = 1.f / static_cast<float>(quickfadeSamples);
}

void Grain::updateCachedLoopBoundaries() {
    const auto numSamples = currentBuffer->buffer.getNumSamples();
    cachedLoopBoundaries.loopStartSample = settings.loopSettings.getLoopStartSample(numSamples);
    cachedLoopBoundaries.loopEndSample = settings.loopSettings.getLoopEndSample(numSamples);
    cachedLoopBoundaries.loopCrossfadeSamples = settings.loopSettings.getCrossfadeSamples(numSamples);
    cachedLoopBoundaries.loopFadeStart = settings.loopSettings.getCrossfadeStartSample(numSamples);
    cachedLoopBoundaries.loopFadeStartReverse = settings.loopSettings.getReverseCrossfadeStartSample(numSamples);
    cachedLoopBoundaries.loopLengthSamples = settings.loopSettings.getLoopLengthSamples(numSamples);
}

float Grain::getGrainShapeGain(float p, float sym, float skew) {
    if (skew < 0)
        p = pow(p, fastexp(skew));
    else if (skew > 0) {
        p = pow(1 - p, fastexp(-skew));
    }

    auto alphaStart = std::max(1 - sym, 0.001f);
    auto alphaEnd = alphaStart;
    auto pInv = 1 - p;

    float v;
    if (2 * pInv <= alphaEnd) {
        v = 0.5f * (1 - fastcos((juce::MathConstants<float>::twoPi * pInv) / alphaEnd));
    } else if (2 * p <= alphaStart) {
        v = 0.5f * (1 - fastcos((juce::MathConstants<float>::twoPi * p) / alphaStart));
    } else v = 1.f;

    jassert(!std::isnan(v));

    return v;
}
