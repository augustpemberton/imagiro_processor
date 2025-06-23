//
// Created by August Pemberton on 23/08/2022.
//
#include "Grain.h"

#include "imagiro_processor/src/dsp/interpolation.h"

Grain::Grain(GrainBuffer& buffer, size_t i) : grainBuffer(buffer), gain(0), pointer(0), sampleRate(0),
                                              indexInStream(i), isLooping(false), windowFunction(), cachedPanCoeffs{} {
}

void Grain::configure(GrainSettings s) {
    settings = s;
    spreadVal = (imagiro::rand01() * 2 - 1) * settings.spread;
    calculatePanCoeffs(settings.pan + spreadVal);

    // if the grain is infinitely long, don't use the window function, just play full volume
    if (s.duration < 0) {
        progressPerSample = 0;
        windowFunction.initialise([s](float p) {
            return 1.f;
        }, 0.f, 1.f, 2);
    } else {
        progressPerSample = 1.f / (s.duration * (float)sampleRate);

        windowFunction.initialise([s](float p) {
            return getGrainShapeGain(p, s.shape, s.skew);
        }, 0.f, 1.f, 100);
    }

    smoothPitchRatio.setCurrentAndTargetValue(s.getPitchRatio());
}

float* Grain::calculatePanCoeffs(const float val) {
    if (val != cachedPan) {
        cachedPanCoeffs[0] = sinApprox((1 - val) * juce::MathConstants<float>::pi / 2);
        cachedPanCoeffs[1] = sinApprox(val * juce::MathConstants<float>::pi / 2);
        cachedPan = val;
    }
    return cachedPanCoeffs;
}


void Grain::play(int sampleDelay) {
    currentBuffer = grainBuffer.getBuffer();
    if (!currentBuffer) return;
    auto spawnPosition = settings.position * static_cast<float>(currentBuffer->getNumSamples());

    if (settings.loopSettings.loopActive) {
        const auto loopStart = settings.loopSettings.getLoopStartSample(currentBuffer->getNumSamples());
        const auto loopEnd = settings.loopSettings.getLoopEndSample(currentBuffer->getNumSamples());
        bool pastLoop = false;
        if (!settings.reverse) {
            pastLoop = spawnPosition >= loopEnd;
        } else {
            pastLoop = spawnPosition <= loopStart;
        }

        if (pastLoop) spawnPosition = imagiro::wrapWithinRange(spawnPosition, loopStart, loopEnd);
    }

    pointer = std::min((double)currentBuffer->getNumSamples() - INTERP_POST_SAMPLES - 1 - quickfadeSamples, pointer);
    pointer = static_cast<double>(std::max((float)INTERP_PRE_SAMPLES, spawnPosition));
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

float Grain::getGrainSpeed() {
    return settings.getPitchRatio();
}

int Grain::getSamplesUntilEndOfBuffer() {
    auto maxPitchRatio = std::max(std::abs(smoothPitchRatio.getCurrentValue()), std::abs(smoothPitchRatio.getTargetValue()));

    int s;
    if (settings.reverse) {
        s = (int)((pointer - INTERP_PRE_SAMPLES) / maxPitchRatio);
    } else {
        s = (int)((currentBuffer->getNumSamples() - INTERP_POST_SAMPLES - pointer) / (maxPitchRatio));
    }

    return s;

    // auto wrapped = imagiro::wrapWithinRange(s, 0, grainBuffer.getNumSamples() / maxPitchRatio) - INTERP_POST_SAMPLES;
    // if (wrapped < 1000) jassertfalse;
    // return wrapped;
}

void Grain::updateLoopSettings(LoopSettings s) {
    if (isLooping) queuedLoopSettings = s;
    else setNewLoopSettingsInternal(s);
}

void Grain::processBlock(juce::AudioSampleBuffer& out, int outStartSample, int numSamples, bool setNotAdd) {
    if (currentBuffer == nullptr) return;

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

    const auto loopStartSample = settings.loopSettings.getLoopStartSample(currentBuffer->getNumSamples());
    const auto loopCrossfadeSamples = settings.loopSettings.getCrossfadeSamples(currentBuffer->getNumSamples());
    const auto loopEndSample = settings.loopSettings.getLoopEndSample(currentBuffer->getNumSamples());
    const auto loopFadeStart = settings.loopSettings.getCrossfadeStartSample(currentBuffer->getNumSamples());
    const auto loopFadeStartReverse = settings.loopSettings.getReverseCrossfadeStartSample(currentBuffer->getNumSamples());
    const auto loopLengthSamples = settings.loopSettings.getLoopLengthSamples(currentBuffer->getNumSamples());

    const auto pastLoopRegion = settings.loopSettings.loopActive && pointer > loopEndSample;

    if (pastLoopRegion || !settings.loopSettings.loopActive) {
        const auto samplesUntilEnd = getSamplesUntilEndOfBuffer() - INTERP_POST_SAMPLES - 1;
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
        auto samplesUntilEndOfGrain = (int)((1 - progress) / progressPerSample);
        numSamples = std::min(numSamples, samplesUntilEndOfGrain);
    }


    if (firstBlockFlag) {
        firstBlockFlag = false;
        smoothPitchRatio.setCurrentAndTargetValue(smoothPitchRatio.getTargetValue());
    }

    while (numSamples > 0 && quickfadeGain > 0.f) {
        auto samplesThisChunk = numSamples;

        auto startPitchRatio = smoothPitchRatio.getCurrentValue();
        smoothPitchRatio.skip(samplesThisChunk);
        auto endPitchRatio = smoothPitchRatio.getCurrentValue();
        auto pitchRatioPerSample = (endPitchRatio - startPitchRatio) / (float) samplesThisChunk;

        auto startPan = smoothedPan.getCurrentValue();
        smoothedPan.skip(samplesThisChunk);
        auto endPan = smoothedPan.getCurrentValue();
        auto panPerSample = (endPan - startPan) / (float) samplesThisChunk;

        auto quickfadeGainStart = quickfadeGain;
        int maxQuickfadeSamples = quickfading ?
                                  static_cast<int>(quickfadeGainStart / quickfadeGainPerSample) + 1 : samplesThisChunk;
        samplesThisChunk = std::min(samplesThisChunk, maxQuickfadeSamples);

        double pos = pointer;
        bool looping = isLooping;
        for (int c=0; c<out.getNumChannels(); c++) {
            auto inChannel = c % currentBuffer->getNumChannels();
            pos = pointer;
            looping = isLooping;

            for (auto s=0; s<samplesThisChunk; s++) {
                auto pitchRatio = startPitchRatio + (float)s * pitchRatioPerSample;
                pos += pitchRatio;

                if (!settings.reverse && settings.loopSettings.loopActive && pos >= loopEndSample) {
                    pos -= loopLengthSamples;
                    pos += loopCrossfadeSamples;
                    looping = false;
                } else if (settings.reverse && settings.loopSettings.loopActive && pos <= loopStartSample) {
                    pos += loopLengthSamples;
                    pos -= loopCrossfadeSamples;
                    looping = false;
                }


                auto v = imagiro::interpolateHQ(*currentBuffer, inChannel, static_cast<float>(pos));

                // only loop if we just crossed the loop fade boundary
                // we don't want to start looping midway through the fade boundary or we'll get clicks
                if (settings.loopSettings.loopActive) {
                    if (!settings.reverse && pos - pitchRatio < loopFadeStart && pos >= loopFadeStart) {
                        looping = true;
                    } else if (settings.reverse && pos - pitchRatio > loopFadeStartReverse && pos <= loopFadeStartReverse) {
                        looping = true;
                    }
                }

                if (looping && loopCrossfadeSamples > 0) {
                    if (!settings.reverse && pos >= loopFadeStart && pos <= loopEndSample + 1) {
                        const auto distanceIntoFade = pos - loopFadeStart;
                        loopFadePointer = loopStartSample + distanceIntoFade;
                        const auto fadeSample = imagiro::interpolateHQ(*currentBuffer, inChannel, static_cast<float>(loopFadePointer));
                        loopFadeProgress = distanceIntoFade / loopCrossfadeSamples;
                        v = v * (1-loopFadeProgress) + fadeSample * loopFadeProgress;
                    } else if (settings.reverse && pos >= loopStartSample - 1 && pos <= loopFadeStartReverse) {
                        const auto distanceIntoFade = loopFadeStartReverse - pos;
                        loopFadePointer = loopEndSample - distanceIntoFade;
                        const auto fadeSample = imagiro::interpolateHQ(*currentBuffer, inChannel, static_cast<float>(loopFadePointer));
                        loopFadeProgress = distanceIntoFade / loopCrossfadeSamples;
                        v = v * (1-loopFadeProgress) + fadeSample * loopFadeProgress;
                    }
                }

                v *= windowFunction(progress + (float)s * progressPerSample);
                v *= settings.gain;

                if (quickfading) {
                    const auto quickfadeAmount = quickfadeGainStart - quickfadeGainPerSample * (float)s;
                    if (quickfadeAmount <= 0.f) break;
                    v *= quickfadeAmount;
                }

                const auto pan = startPan + static_cast<float>(s) * panPerSample;
                v *= currentBuffer->getNumChannels() > 1 ? calculatePanCoeffs(pan + spreadVal)[c%2] : 1.f;

                if (setNotAdd) out.setSample(c, outStartSample + s, v);
                else out.addSample(c, outStartSample + s, v);
            }
        }

        pointer = pos;
        isLooping = looping;
        progress += progressPerSample * (float)samplesThisChunk;
        if (quickfading) quickfadeGain -= quickfadeGainPerSample * (float)samplesThisChunk;

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

void Grain::setNewLoopSettingsInternal(const LoopSettings loopSettings) {
    settings.loopSettings = loopSettings;

    // check if we've just gone over the current position
    const auto loopStartSample = settings.loopSettings.getLoopStartSample(currentBuffer->getNumSamples());
    const auto loopCrossfadeSamples = settings.loopSettings.getCrossfadeSamples(currentBuffer->getNumSamples());
    const auto loopEndSample = settings.loopSettings.getLoopEndSample(currentBuffer->getNumSamples());
    const auto loopFadeStart = settings.loopSettings.getCrossfadeStartSample(currentBuffer->getNumSamples());
    const auto loopLengthSamples = settings.loopSettings.getLoopLengthSamples(currentBuffer->getNumSamples());

    auto newLengthSamples = loopLengthSamples;
    auto newLoopCrossfadeSamples = loopCrossfadeSamples;

    // adding a little padding to compensate for float rounding
    const auto nextPointerPos = pointer + getCurrentPitchRatio();
    const auto bufferStart = INTERP_PRE_SAMPLES;
    const auto bufferEnd = currentBuffer->getNumSamples() - 1 - INTERP_POST_SAMPLES;

    if (!settings.reverse) {
        auto minLoopEnd = std::min(bufferEnd, static_cast<int>(nextPointerPos + loopCrossfadeSamples));
        minLoopEnd += 1; // compensate for rounding errors
        if (loopEndSample < minLoopEnd) {
            newLoopCrossfadeSamples = std::min(newLoopCrossfadeSamples, minLoopEnd - static_cast<int>(nextPointerPos+1));
            newLengthSamples = minLoopEnd - loopStartSample;
            const auto newLengthPercentage = newLengthSamples / static_cast<float>(currentBuffer->getNumSamples());
            settings.loopSettings.loopLength = newLengthPercentage;
        }
    } else {
        auto maxLoopStart = std::max(bufferStart, static_cast<int>(nextPointerPos - loopCrossfadeSamples));
        maxLoopStart -= 1; // compensate for rounding errors
        if (loopStartSample > maxLoopStart) {
            newLoopCrossfadeSamples = std::min(newLoopCrossfadeSamples, static_cast<int>(nextPointerPos) - maxLoopStart);
            newLengthSamples = loopEndSample - maxLoopStart;

            const auto newStartPercentage = maxLoopStart / static_cast<float>(currentBuffer->getNumSamples());
            settings.loopSettings.loopStart = newStartPercentage;

            const auto newLengthPercentage = newLengthSamples / static_cast<float>(currentBuffer->getNumSamples());
            settings.loopSettings.loopLength = newLengthPercentage;
        }
    }


    if (newLengthSamples != loopLengthSamples || newLoopCrossfadeSamples != loopCrossfadeSamples) {
        // update crossfade percentage to keep the same number of samples
        const auto newCrossfadePercentage = newLoopCrossfadeSamples / static_cast<float>(newLengthSamples) * 2;
        settings.loopSettings.loopCrossfade = newCrossfadePercentage;
    }
}


void Grain::stop(bool fadeout) {
    if (fadeout) stopFlag = true;
    else {
        playing = false;
        currentBuffer.reset();
        listeners.call(&Listener::OnGrainFinished, *this);
    }
}

void Grain::prepareToPlay(double sr, int maxBlockSize) {
    this->sampleRate = sr;
    temp.setSize(1, maxBlockSize);

    smoothPitchRatio.reset(sr, 0.01);
    smoothedPan.reset(sr, 0.01);

    quickfadeSamples = (int)(quickfadeSeconds * (float)sr);
    quickfadeGainPerSample = 1.f / static_cast<float>(quickfadeSamples);
}

float Grain::getGrainShapeGain(float p, float sym, float skew) {

    if (skew < 0)
        p = pow(p, exp(skew));
    else if (skew > 0) {
        p = pow(1-p, exp(-skew));
    }

    auto alphaStart = std::max(1 - sym, 0.001f);
    auto alphaEnd = alphaStart;
    auto pInv = 1-p;

    float v;
    if (2 * pInv <= alphaEnd) {
        v = 0.5f * (1-std::cos((juce::MathConstants<float>::twoPi * pInv) / alphaEnd));
    } else if (2 * p <= alphaStart) {
        v = 0.5f * (1-std::cos((juce::MathConstants<float>::twoPi * p) / alphaStart));
    } else v = 1.f;

    jassert(!std::isnan(v));

    return v;
}
