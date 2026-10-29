// include/ccsim/sim/types.hpp
//
// Shared "vocabulary" of the simulator: the small data types that the engine
// (Role 2), the replacement policies (Role 3), the trace reader (Role 6) and the
// CLI all use to talk to each other. This file holds data only, no cache logic.
//
// Source of truth: docs/TECHNICAL_DESIGN.md section 9 ("Module interfaces").
// Names, field order and defaults match that sketch exactly. Do not rename or add
// fields without Role 1's approval (this contract freezes at the end of Milestone 1).
//
// Note: the namespace is `ccsim`, NOT `ccsim::sim`. The folder is called "sim",
// but the design puts these types directly in `ccsim`.

#pragma once  // "include me only once per .cpp file", prevents duplicate-definition errors

#include <cstdint>    // std::uint8_t, std::uint32_t, std::uint64_t (exact-size integers)
#include <optional>   // std::optional (a value that may be absent)
#include <stdexcept>  // std::runtime_error
#include <string>     // std::string

namespace ccsim {

// Is the CPU reading memory (load) or writing it (store)?
// `enum class` = a named set of choices that cannot be mixed up with plain ints.
enum class AccessType : std::uint8_t { Read, Write };

// One line of a trace: "R 0x7ffd1a20" becomes {AccessType::Read, 0x7ffd1a20}.
// The address is a BYTE address; the cache works out which line it belongs to.
struct MemoryAccess {
    AccessType type = AccessType::Read;
    std::uint64_t address = 0;
};

// Which replacement policy a cache uses (what to throw out when a set is full).
enum class PolicyKind : std::uint8_t { Lru, Fifo, Random };

// Settings for ONE cache level (L1 or L2).
struct CacheConfig {
    std::string name;                       // "L1" or "L2"
    std::uint64_t size_bytes = 0;           // total capacity in bytes
    std::uint32_t line_bytes = 0;           // bytes per cache line; must be a power of two
    std::uint32_t ways = 0;                 // lines per set; 1 = direct-mapped
    std::uint32_t hit_latency_cycles = 0;   // cycles for a hit; at least 1
    PolicyKind policy = PolicyKind::Lru;
};

// Settings for the whole memory system.
struct HierarchyConfig {
    CacheConfig l1;
    std::optional<CacheConfig> l2;               // empty (std::nullopt) = no L2; L1 talks to memory
    std::uint32_t memory_latency_cycles = 0;     // cycles for a main-memory access; at least 1
    std::uint64_t seed = 1;                      // used by the Random policy only
};

// Counters for one cache level. All start at 0 and only ever count up.
struct LevelStats {
    std::uint64_t accesses = 0, reads = 0, writes = 0;
    std::uint64_t hits = 0, misses = 0, read_misses = 0, write_misses = 0;
    std::uint64_t evictions = 0, writebacks = 0;
    // L2 only: dirty lines that L1 pushed down into this level.
    std::uint64_t writebacks_received = 0, writeback_hits = 0, writeback_misses = 0;
};

// Main-memory traffic counters.
struct MemoryStats {
    std::uint64_t reads = 0, writes = 0;
};

// The error thrown when a configuration is invalid.
// Deriving from std::runtime_error lets the CLI catch it and exit with code 1.
struct ConfigError : std::runtime_error {
    using std::runtime_error::runtime_error;  // reuse the base class constructors
};

// Checks every rule from TECHNICAL_DESIGN.md section 3. Throws ConfigError, naming
// the offending field, if anything is wrong. (Declared here; defined in src/sim/.)
void validate(const HierarchyConfig& config);

}  // namespace ccsim
