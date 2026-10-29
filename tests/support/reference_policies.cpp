#include "reference_policies.hpp"

#include <random>
#include <stdexcept>
#include <vector>

namespace ccsim::testing {
namespace {

// Per-slot stamp; LRU stamps on hit and fill, FIFO on fill only. Victim = smallest stamp.
class StampPolicy final : public ReplacementPolicy {
public:
    StampPolicy(std::uint32_t sets, std::uint32_t ways, bool stamp_on_hit)
        : ways_(ways), stamp_on_hit_(stamp_on_hit), stamps_(static_cast<std::size_t>(sets) * ways, 0) {}

    void on_hit(std::uint32_t set, std::uint32_t way) override {
        if (stamp_on_hit_) stamps_[index(set, way)] = ++clock_;
    }
    void on_fill(std::uint32_t set, std::uint32_t way) override { stamps_[index(set, way)] = ++clock_; }
    std::uint32_t select_victim(std::uint32_t set) override {
        std::uint32_t best = 0;
        for (std::uint32_t w = 1; w < ways_; ++w) {
            if (stamps_[index(set, w)] < stamps_[index(set, best)]) best = w;
        }
        return best;
    }

private:
    std::size_t index(std::uint32_t set, std::uint32_t way) const noexcept {
        return static_cast<std::size_t>(set) * ways_ + way;
    }
    std::uint32_t ways_;
    bool stamp_on_hit_;
    std::uint64_t clock_ = 0;
    std::vector<std::uint64_t> stamps_;
};

// mt19937_64 reduced with % ways (never uniform_int_distribution: implementation-defined).
class RandomPolicy final : public ReplacementPolicy {
public:
    RandomPolicy(std::uint32_t ways, std::uint64_t seed) : ways_(ways), rng_(seed) {}
    void on_hit(std::uint32_t, std::uint32_t) override {}
    void on_fill(std::uint32_t, std::uint32_t) override {}
    std::uint32_t select_victim(std::uint32_t) override { return static_cast<std::uint32_t>(rng_() % ways_); }

private:
    std::uint32_t ways_;
    std::mt19937_64 rng_;
};

}  // namespace

std::unique_ptr<ReplacementPolicy> make_reference_policy(PolicyKind kind, std::uint32_t sets,
                                                         std::uint32_t ways, std::uint64_t seed) {
    switch (kind) {
        case PolicyKind::Lru: return std::make_unique<StampPolicy>(sets, ways, true);
        case PolicyKind::Fifo: return std::make_unique<StampPolicy>(sets, ways, false);
        case PolicyKind::Random: return std::make_unique<RandomPolicy>(ways, seed);
    }
    throw std::invalid_argument("make_reference_policy: unknown PolicyKind");
}

}  // namespace ccsim::testing
