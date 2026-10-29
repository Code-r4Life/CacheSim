// include/ccsim/sim/cache_level.hpp
//
// Storage and bookkeeping for ONE cache level. Pure mechanism: the Hierarchy decides
// what happens on a miss (routing, inclusion, write policy); this class only holds lines,
// finds them, picks a way (invalid-first, else the policy's victim) and counts per set.
//
// Layout: struct of arrays indexed by slot = set * ways + way.
//   tags_  : uint64 per slot
//   state_ : uint8 per slot (bit 0 valid, bit 1 dirty)
//   filled_: uint32 per set, number of valid ways (skips the invalid-way scan when full)
// Lookup is a linear scan for ways <= 16 (fits one or two cache lines of tags); above that
// a LineIndex gives O(1) lookups so fully associative caches stay fast.

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "ccsim/policy/replacement_policy.hpp"
#include "ccsim/sim/geometry.hpp"
#include "ccsim/sim/line_index.hpp"
#include "ccsim/sim/types.hpp"

namespace ccsim {

inline constexpr std::uint32_t kNoWay = 0xFFFFFFFFU;

// A line that was removed from a slot.
struct Victim {
    bool valid = false;
    bool dirty = false;
    std::uint64_t line = 0;
};

class CacheLevel {
public:
    static constexpr std::uint32_t kLinearScanMaxWays = 16;

    CacheLevel(const CacheConfig& config, std::unique_ptr<ReplacementPolicy> policy);

    const CacheConfig& config() const noexcept { return config_; }
    const Geometry& geometry() const noexcept { return geo_; }

    // Way holding `line` in `set`, or kNoWay.
    std::uint32_t find(std::uint32_t set, std::uint64_t line) const noexcept {
        if (index_) {
            const std::uint32_t slot = index_->find(line);
            return slot == LineIndex::kAbsent ? kNoWay : slot - set * geo_.ways;
        }
        const std::uint64_t tag = geo_.tag_of(line);
        const std::size_t base = static_cast<std::size_t>(set) * geo_.ways;
        for (std::uint32_t w = 0; w < geo_.ways; ++w) {
            if ((state_[base + w] & kValid) != 0 && tags_[base + w] == tag) return w;
        }
        return kNoWay;
    }

    // Lowest invalid way, else the policy's victim (TECHNICAL_DESIGN 5.1 step 2).
    // Sets `*from_policy` to whether the policy was consulted.
    std::uint32_t choose_way(std::uint32_t set, bool* from_policy);

    // Removes whatever occupies (set, way) and returns it; the slot becomes invalid.
    Victim take(std::uint32_t set, std::uint32_t way) noexcept;

    // Installs `line` into an invalid slot and notifies the policy (on_fill).
    void install(std::uint32_t set, std::uint32_t way, std::uint64_t line, bool dirty);

    void touch(std::uint32_t set, std::uint32_t way) { policy_->on_hit(set, way); }
    void mark_dirty(std::uint32_t set, std::uint32_t way) noexcept {
        state_[slot(set, way)] |= kDirty;
    }
    bool is_dirty(std::uint32_t set, std::uint32_t way) const noexcept {
        return (state_[slot(set, way)] & kDirty) != 0;
    }
    bool is_valid(std::uint32_t set, std::uint32_t way) const noexcept {
        return (state_[slot(set, way)] & kValid) != 0;
    }
    std::uint64_t tag_at(std::uint32_t set, std::uint32_t way) const noexcept {
        return tags_[slot(set, way)];
    }

    void count_access(std::uint32_t set) noexcept { ++set_accesses_[set]; }
    void count_miss(std::uint32_t set) noexcept { ++set_misses_[set]; }
    const std::vector<std::uint64_t>& set_accesses() const noexcept { return set_accesses_; }
    const std::vector<std::uint64_t>& set_misses() const noexcept { return set_misses_; }

    // Raw slot state for UI snapshots: bit 0 valid, bit 1 dirty.
    const std::vector<std::uint8_t>& slot_states() const noexcept { return state_; }
    const std::vector<std::uint64_t>& slot_tags() const noexcept { return tags_; }

    std::size_t memory_bytes() const noexcept;

private:
    static constexpr std::uint8_t kValid = 1;
    static constexpr std::uint8_t kDirty = 2;

    std::size_t slot(std::uint32_t set, std::uint32_t way) const noexcept {
        return static_cast<std::size_t>(set) * geo_.ways + way;
    }

    CacheConfig config_;
    Geometry geo_;
    std::unique_ptr<ReplacementPolicy> policy_;
    std::vector<std::uint64_t> tags_;
    std::vector<std::uint8_t> state_;
    std::vector<std::uint32_t> filled_;
    std::vector<std::uint64_t> set_accesses_;
    std::vector<std::uint64_t> set_misses_;
    std::optional<LineIndex> index_;
};

}  // namespace ccsim
