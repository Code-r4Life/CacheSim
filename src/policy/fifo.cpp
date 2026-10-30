// src/policy/fifo.cpp
//
// FIFO = First In, First Out: throw out the line that has been in the cache the longest,
// no matter how often it was used since. Hits change nothing (ARCHITECTURE 5.6).
//
// How: identical to LRU's stamps, except that only a FILL writes a stamp. A hit does not.
// The victim is the slot with the smallest stamp, i.e. the one installed first.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "policy_impl.hpp"

namespace ccsim {
namespace {

class FifoPolicy final : public ReplacementPolicy {
public:
    FifoPolicy(std::uint32_t sets, std::uint32_t ways)
        : ways_(ways), stamp_(static_cast<std::size_t>(sets) * ways, 0) {}

    // Deliberately empty: using a line must not change its place in the queue.
    void on_hit(std::uint32_t, std::uint32_t) override {}

    // Record "installed now". Overwrites any stale stamp left by a silently removed line.
    void on_fill(std::uint32_t set, std::uint32_t way) override {
        stamp_[static_cast<std::size_t>(set) * ways_ + way] = ++clock_;
    }

    std::uint32_t select_victim(std::uint32_t set) override {
        const std::size_t base = static_cast<std::size_t>(set) * ways_;
        std::uint32_t victim = 0;
        for (std::uint32_t w = 1; w < ways_; ++w) {
            if (stamp_[base + w] < stamp_[base + victim]) victim = w;  // installed earlier wins
        }
        return victim;
    }

private:
    std::uint32_t ways_;
    std::vector<std::uint64_t> stamp_;
    std::uint64_t clock_ = 0;
};

}  // namespace

std::unique_ptr<ReplacementPolicy> make_fifo_policy(std::uint32_t sets, std::uint32_t ways) {
    return std::make_unique<FifoPolicy>(sets, ways);
}

}  // namespace ccsim
