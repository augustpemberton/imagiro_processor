#pragma once

#include <imagiro_util/structures/beman/inplace_vector.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace imagiro {

// Voice allocation policy (from MPESynth): first-free-slot search, oldest-voice
// stealing once the active count reaches maxVoices, and noteId -> voice mapping
// for release. Pure decisions over a read-view of voice slot state — owns no
// voices and does no rendering, so it is entirely DSP-agnostic. MaxVoices is the
// physical slot-pool size; half is held in reserve so a steal can crossfade
// without clicks.
template <std::size_t MaxVoices>
class VoiceAllocator {
public:
    struct VoiceSlot {
        int noteId = 0;
        uint64_t startOrder = 0;
        bool hasNote = false;
        bool stealFading = false;
    };

    using Slots = std::array<VoiceSlot, MaxVoices>;

    struct NoteOnDecision {
        std::optional<std::size_t> startIndex;
        std::optional<std::size_t> stealIndex;
    };

    void setMaxVoices(int maxVoices) {
        maxVoices_ = std::min(static_cast<int>(MaxVoices / 2), maxVoices);
    }

    int getMaxVoices() const { return maxVoices_; }

    NoteOnDecision noteOn(const Slots& slots) const {
        NoteOnDecision decision;
        for (std::size_t i = 0; i < slots.size(); i++) {
            if (!slots[i].hasNote) {
                decision.startIndex = i;
                break;
            }
        }
        if (!decision.startIndex) return decision;

        int activeCount = 0;
        for (const auto& slot : slots)
            if (slot.hasNote) activeCount++;

        if (activeCount >= maxVoices_) {
            uint64_t oldestOrder = 0;
            for (std::size_t i = 0; i < slots.size(); i++) {
                const auto& slot = slots[i];
                if (!slot.hasNote || slot.stealFading) continue;
                if (!decision.stealIndex || slot.startOrder < oldestOrder) {
                    decision.stealIndex = i;
                    oldestOrder = slot.startOrder;
                }
            }
        }
        return decision;
    }

    beman::inplace_vector<std::size_t, MaxVoices>
    noteOff(const Slots& slots, int noteId) const {
        beman::inplace_vector<std::size_t, MaxVoices> stopIndices;
        for (std::size_t i = 0; i < slots.size(); i++) {
            if (slots[i].hasNote && slots[i].noteId == noteId)
                stopIndices.push_back(i);
        }
        return stopIndices;
    }

private:
    int maxVoices_ = MaxVoices / 2;
};

} // namespace imagiro
