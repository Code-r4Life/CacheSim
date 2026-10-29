// include/ccsim/policy/replacement_policy.hpp
//
// The contract for "which line do we throw out when a set is full?".
// A cache level owns one ReplacementPolicy object and tells it what happens;
// the policy only keeps its own bookkeeping and answers one question: the victim.
//
// Source of truth: docs/TECHNICAL_DESIGN.md sections 5.4 and 9. Signatures match
// that sketch exactly. Changing them needs Role 1's approval (contract freeze, M1).
//
// Vocabulary: a cache is split into `sets`; each set has `ways` slots, and each slot
// can hold one line. A policy identifies a line by its (set, way) position, never by
// address, so a policy never needs to know about tags or memory.

#pragma once

#include <cstdint>  // std::uint32_t, std::uint64_t
#include <memory>   // std::unique_ptr

#include "ccsim/sim/types.hpp"  // PolicyKind

namespace ccsim {

// Abstract base class ("interface"): the `= 0` methods have no body here. Each real
// policy (LRU, FIFO, Random, ...) is a class that derives from this and fills them in.
// The cache only ever talks to a ReplacementPolicy, so it does not care which one.
class ReplacementPolicy {
public:
    // Virtual destructor: lets a std::unique_ptr<ReplacementPolicy> delete the real
    // (derived) object correctly.
    virtual ~ReplacementPolicy() = default;

    // The line at (set, way) was just HIT (the CPU found it in the cache).
    virtual void on_hit(std::uint32_t set, std::uint32_t way) = 0;

    // A new line was just INSTALLED at (set, way) after a miss.
    virtual void on_fill(std::uint32_t set, std::uint32_t way) = 0;

    // Pick the way to evict from `set`. The cache calls this ONLY when every way in
    // the set holds a valid line (empty ways are used first, without asking the policy).
    // Must return a value in [0, ways).
    virtual std::uint32_t select_victim(std::uint32_t set) = 0;
};

// Builds the policy for one cache level. `sets` and `ways` size its bookkeeping.
// `seed` is the seed for THIS level; only Random uses it. Design section 5.4 says the
// level's generator is seeded with `seed + level_index` (L1 = 0, L2 = 1), so the caller
// (the hierarchy) passes that already-offset value. Defined later in src/policy/.
std::unique_ptr<ReplacementPolicy> make_policy(PolicyKind kind, std::uint32_t sets,
                                               std::uint32_t ways, std::uint64_t seed);

}  // namespace ccsim
