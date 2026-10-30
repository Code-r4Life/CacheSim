// include/ccsim/sim/geometry.hpp
//
// Address mapping for one cache level (TECHNICAL_DESIGN section 4):
//   line = address >> offset_bits;  set = line & (sets - 1);  tag = line >> index_bits
// All shifts and masks, no division on the hot path.

#pragma once

#include <cstdint>

#include "ccsim/sim/types.hpp"

namespace ccsim {

struct Geometry {
    std::uint32_t line_bytes = 0;
    std::uint32_t ways = 0;
    std::uint32_t sets = 0;
    std::uint32_t offset_bits = 0;
    std::uint32_t index_bits = 0;
    std::uint64_t set_mask = 0;

    // Derives the geometry and enforces the rules the engine depends on for memory
    // safety (power-of-two line and set count, exact division, ways >= 1). Throws
    // ConfigError naming `config.name`. validate() remains the full user-facing rule set.
    static Geometry from(const CacheConfig& config);

    std::uint64_t line_of(std::uint64_t address) const noexcept { return address >> offset_bits; }
    std::uint32_t set_of(std::uint64_t line) const noexcept {
        return static_cast<std::uint32_t>(line & set_mask);
    }
    std::uint64_t tag_of(std::uint64_t line) const noexcept { return line >> index_bits; }
    std::uint32_t offset_of(std::uint64_t address) const noexcept {
        return static_cast<std::uint32_t>(address & (std::uint64_t{line_bytes} - 1));
    }
    std::uint64_t line_from(std::uint64_t tag, std::uint32_t set) const noexcept {
        return (tag << index_bits) | set;
    }
    std::uint64_t capacity_lines() const noexcept { return std::uint64_t{sets} * ways; }
};

}  // namespace ccsim
