// JuceParamAdapter.h
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ParamController.h"
#include <imagiro_util/readerwriterqueue/concurrentqueue.h>

namespace imagiro {
    class JuceParamAdapter : juce::AudioParameterFloat::Listener {
    public:
        JuceParamAdapter(ParamController &controller, juce::AudioProcessor &processor)
            : controller_(controller) {
            controller_.forEach([&](Handle h, const ParamConfig &config) {
                if (config.isInternal) {
                    // Internal params: not exposed to the DAW host.
                    // Store nullptr so handle-indexed lookups still work.
                    juceParams_.push_back(nullptr);
                    paramConnections_.emplace_back();
                    return;
                }

                auto param = std::make_unique<juce::AudioParameterFloat>(
                    juce::ParameterID{config.uid, 1},
                    config.name,
                    juce::NormalisableRange<float>(0.f, 1.f),
                    config.range.normalize(config.defaultValue),
                    "",
                    juce::AudioProcessorParameter::genericParameter,
                    [&config](float val, int) {
                        return config.format.toString(config.range.denormalize(val));
                    },
                    [&config](const juce::String &text) {
                        const auto parsed = config.format.fromString(text.toStdString());
                        return parsed ? config.range.normalize(*parsed) : 0.f;
                    }
                );

                param->addListener(this);

                auto *rawPtr = param.get();
                juceParams_.push_back(rawPtr);

                // Map JUCE parameter index back to our Handle
                juceIndexToHandle_.push_back(h);

                paramConnections_.push_back(controller.uiSignal(h).connect_scoped([this, h](float) {
                    pushToHost(h);
                }));

                processor.addParameter(param.release());
            });
        }

        void parameterValueChanged(int parameterIndex, float /*newValue*/) override {
            if (parameterIndex >= 0 && parameterIndex < static_cast<int>(juceIndexToHandle_.size())) {
                paramsToPullFromHost_.enqueue(juceIndexToHandle_[parameterIndex].index);
            }
        }

        void parameterGestureChanged(int, bool) override {
        }

        void pullFromHost() {
            size_t index;
            while (paramsToPullFromHost_.try_dequeue(index)) {
                const auto handle = Handle{static_cast<uint32_t>(index)};
                if (juceParams_[index])
                    controller_.setValue01(handle, juceParams_[index]->get());
            }
        }

        void pushToHost(Handle h) const {
            if (!juceParams_[h.index]) return;  // internal param
            const float normalized = controller_.getValue01UI(h);
            if (std::abs(juceParams_[h.index]->get() - normalized) < 1e-6f) return;
            juceParams_[h.index]->setValueNotifyingHost(normalized);
        }

        void beginGesture(Handle h) const {
            if (!juceParams_[h.index]) return;  // internal param
            juceParams_[h.index]->beginChangeGesture();
        }

        void endGesture(Handle h) const {
            if (!juceParams_[h.index]) return;  // internal param
            juceParams_[h.index]->endChangeGesture();
        }

        void setValueAsUserAction(Handle h, float userValue) const {
            beginGesture(h);
            controller_.setValue(h, userValue);
            pushToHost(h);
            endGesture(h);
        }

        void setValue01AsUserAction(Handle h, float normalized) const {
            beginGesture(h);
            controller_.setValue01(h, normalized);
            pushToHost(h);
            endGesture(h);
        }

    private:
        ParamController &controller_;
        std::vector<juce::AudioParameterFloat *> juceParams_;   // indexed by Handle, nullptr for internal
        std::vector<Handle> juceIndexToHandle_;                  // maps JUCE param index → Handle
        std::vector<sigslot::scoped_connection> paramConnections_;
        moodycamel::ConcurrentQueue<size_t> paramsToPullFromHost_{10};
    };
} // namespace imagiro
