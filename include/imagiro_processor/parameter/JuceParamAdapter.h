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

                paramConnections_.push_back(controller.uiSignal(h).connect_scoped([this, h](float) {
                    pushToHost(h);
                }));

                processor.addParameter(param.release());
            });
        }

        void parameterValueChanged(int parameterIndex, float /*newValue*/) override {
            paramsToPullFromHost_.enqueue(static_cast<size_t>(parameterIndex));
        }

        void parameterGestureChanged(int, bool) override {
        }

        void pullFromHost() {
            size_t index;
            while (paramsToPullFromHost_.try_dequeue(index)) {
                const auto handle = Handle{static_cast<uint32_t>(index)};
                const float normalized = juceParams_[index]->get();
                controller_.setValue01(handle, normalized);
            }
        }

        void pushToHost(Handle h) const {
            const float normalized = controller_.getValue01(h);
            if (std::abs(juceParams_[h.index]->get() - normalized) < 1e-6f) return;
            juceParams_[h.index]->setValueNotifyingHost(normalized);
        }

        void beginGesture(Handle h) const {
            juceParams_[h.index]->beginChangeGesture();
        }

        void endGesture(Handle h) const {
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
        std::vector<juce::AudioParameterFloat *> juceParams_;
        std::vector<sigslot::scoped_connection> paramConnections_;
        moodycamel::ConcurrentQueue<size_t> paramsToPullFromHost_{10};
    };
} // namespace imagiro
