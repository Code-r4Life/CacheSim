// include/ccsim/sim/events.hpp
//
// The per-access record the live visualiser renders (docs/ARCHITECTURE.md section 7).
//
// An AccessEvent is a fixed-size POD: no heap, so recording one costs a few stores and
// the batch path (event == nullptr) pays nothing. `actions` is the ordered micro-step log
// of everything the hierarchy did for this access, in the normative order of
// TECHNICAL_DESIGN 5.2 and ARCHITECTURE 5.3. The frontend animates actions one by one,
// which is how "request travels L1 -> L2 -> memory, dirty victim travels back down"
// is shown without the UI knowing any cache rules.

#pragma once

#include <array>
#include <cstdint>

#include "ccsim/sim/types.hpp"

namespace ccsim {

// Level ids used in events. Memory is level 2 even when there is no L2.
enum class LevelId : std::uint8_t { L1 = 0, L2 = 1, Memory = 2 };

enum class Outcome : std::uint8_t {
    NotAccessed = 0,  // the level was not consulted for the demand access
    Hit = 1,
    Miss = 2,         // missed and allocated
    MissNoAllocate = 3,  // write miss under write-around: forwarded, not installed
};

enum class ActionKind : std::uint8_t {
    Lookup = 0,          // demand lookup at `level` (flag kHit tells the result)
    Evict = 1,           // victim removed from (set, way) to make room
    Fill = 2,            // `line` installed at (set, way)
    Writeback = 3,       // dirty full line arrives at `level` (flag kHit: it was resident)
    WriteThrough = 4,    // partial store arrives at `level` (flag kHit: it was resident)
    BackInvalidate = 5,  // inclusive: line removed from L1 because L2 evicted it
    MoveOut = 6,         // exclusive: line leaves L2 to move up into L1
    VictimInsert = 7,    // exclusive: L1 victim arrives in L2 (flag kDirty if dirty)
    MemoryRead = 8,
    MemoryWrite = 9,
};

struct Action {
    static constexpr std::uint8_t kHit = 1;
    static constexpr std::uint8_t kDirty = 2;
    static constexpr std::uint8_t kFromPolicy = 4;  // Evict: way chosen by the policy

    ActionKind kind = ActionKind::Lookup;
    LevelId level = LevelId::L1;
    std::uint8_t flags = 0;
    std::uint32_t set = 0;
    std::uint32_t way = 0;
    std::uint64_t line = 0;
};

struct LevelView {
    Outcome outcome = Outcome::NotAccessed;
    std::uint32_t set = 0;
    std::uint64_t tag = 0;
    std::uint32_t way = 0;  // way hit or filled; meaningless for NotAccessed/MissNoAllocate
};

struct AccessEvent {
    static constexpr std::size_t kMaxActions = 24;

    std::uint64_t seq = 0;  // 0-based index in the trace
    MemoryAccess access;
    std::uint64_t line = 0;    // address >> offset_bits (L1 and L2 share the line size)
    std::uint32_t offset = 0;  // byte offset inside the line
    LevelView l1;
    LevelView l2;
    std::uint32_t latency_cycles = 0;
    std::uint8_t action_count = 0;
    std::array<Action, kMaxActions> actions{};
};

// Cheap summary returned for every access, even in batch mode; analytics use it.
struct AccessSummary {
    std::uint64_t line = 0;
    Outcome l1 = Outcome::NotAccessed;
    Outcome l2 = Outcome::NotAccessed;
    std::uint32_t latency_cycles = 0;
};

}  // namespace ccsim
