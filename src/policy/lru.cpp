// src/policy/lru.cpp
//
// LRU = Least Recently Used: when a set is full, throw out the line that was touched
// longest ago. Both a hit and a fill count as "touched" (ARCHITECTURE 5.6).
//
// How: every (set, way) slot has a number, its "stamp". A global counter `clock_` goes up by
// one on every touch, and the touched slot gets the new value. The victim is the slot in
// the set with the SMALLEST stamp, i.e. the one touched longest ago. Cost: O(ways) only
// when choosing a victim, O(1) on hits and fills.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "policy_impl.hpp"

namespace ccsim {
namespace {  // private to this file

class LruPolicy final : public ReplacementPolicy {
public:
    LruPolicy(std::uint32_t sets, std::uint32_t ways)
        : ways_(ways), stamp_(static_cast<std::size_t>(sets) * ways, 0) {}

    void on_hit(std::uint32_t set, std::uint32_t way) override { touch(set, way); }

    // A fill may land on a slot that still holds an old stamp (the engine can remove lines
    // silently), so we always OVERWRITE the stamp and never assume it is fresh.
    void on_fill(std::uint32_t set, std::uint32_t way) override { touch(set, way); }

    std::uint32_t select_victim(std::uint32_t set) override {
        const std::size_t base = static_cast<std::size_t>(set) * ways_;  // first slot of this set
        std::uint32_t victim = 0;
        for (std::uint32_t w = 1; w < ways_; ++w) {
            if (stamp_[base + w] < stamp_[base + victim]) victim = w;  // older touch wins
        }
        return victim;  // always in [0, ways)
    }

private:
    void touch(std::uint32_t set, std::uint32_t way) {
        stamp_[static_cast<std::size_t>(set) * ways_ + way] = ++clock_;
    }

    std::uint32_t ways_;                 // slots per set
    std::vector<std::uint64_t> stamp_;   // sets * ways numbers; slot (s, w) is stamp_[s * ways + w]
    std::uint64_t clock_ = 0;            // goes up on every touch, so stamps are unique
};

}  // namespace

std::unique_ptr<ReplacementPolicy> make_lru_policy(std::uint32_t sets, std::uint32_t ways) {
    return std::make_unique<LruPolicy>(sets, ways);
}

}  // namespace ccsim
