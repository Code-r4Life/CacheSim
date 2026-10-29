// include/ccsim/analytics/miss_classifier.hpp
//
// 3C miss classification (Hill & Smith, 1989) for one cache level, computed online:
//   compulsory: first ever reference to the line
//   capacity  : would also miss in a fully associative LRU cache of the same capacity
//   conflict  : would have HIT in that fully associative cache (set mapping / policy cost)
// Feed it every DEMAND access of the level in order, with the real outcome.
// Cost per access: two hash lookups and an O(1) list splice; memory O(unique lines).

#pragma once

#include <cstdint>
#include <vector>

#include "ccsim/sim/line_index.hpp"

namespace ccsim {

struct MissBreakdown {
    std::uint64_t compulsory = 0;
    std::uint64_t capacity = 0;
    std::uint64_t conflict = 0;
    // Real hits that the fully associative LRU shadow missed (the real organisation or
    // policy beat FA-LRU). Reported so the three classes stay honest, never negative.
    std::uint64_t shadow_only_misses = 0;
};

// Fully associative true-LRU cache of `capacity` lines (intrusive list over arrays).
class FullyAssociativeLru {
public:
    explicit FullyAssociativeLru(std::uint64_t capacity_lines);
    // Returns true on hit. Always leaves `line` as most recently used.
    bool access(std::uint64_t line);
    std::size_t memory_bytes() const noexcept;

private:
    static constexpr std::uint32_t kNil = 0xFFFFFFFFU;
    void unlink(std::uint32_t n) noexcept;
    void push_front(std::uint32_t n) noexcept;

    std::uint32_t capacity_;
    std::uint32_t used_ = 0;
    std::uint32_t head_ = kNil, tail_ = kNil;
    std::vector<std::uint64_t> line_;
    std::vector<std::uint32_t> prev_, next_;
    LineIndex where_;
};

class MissClassifier {
public:
    explicit MissClassifier(std::uint64_t capacity_lines) : shadow_(capacity_lines) {}

    void observe(std::uint64_t line, bool real_hit);
    const MissBreakdown& breakdown() const noexcept { return counts_; }
    std::size_t memory_bytes() const noexcept { return shadow_.memory_bytes() + seen_.memory_bytes(); }

private:
    FullyAssociativeLru shadow_;
    LineIndex seen_;
    MissBreakdown counts_;
};

}  // namespace ccsim
