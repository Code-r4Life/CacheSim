// src/policy/random.cpp
//
// Random: throw out a randomly chosen line. It remembers nothing about the lines.
//
// The recipe is fixed by the design (ARCHITECTURE 5.6) so results are identical on every
// machine and browser: one std::mt19937_64 generator per cache level, seeded with the seed
// we are given, and the victim is `rng() % ways`. The generator moves forward ONLY when a
// victim is needed. We never use std::uniform_int_distribution: the standard lets every
// compiler implement it differently, which would break reproducibility.

#include <cstdint>
#include <memory>
#include <random>

#include "policy_impl.hpp"

namespace ccsim {
namespace {

class RandomPolicy final : public ReplacementPolicy {
public:
    RandomPolicy(std::uint32_t ways, std::uint64_t seed) : ways_(ways), rng_(seed) {}

    // Nothing to remember, so nothing to do (and the generator must not advance here).
    void on_hit(std::uint32_t, std::uint32_t) override {}
    void on_fill(std::uint32_t, std::uint32_t) override {}

    // `set` is unused: all sets share the level's one generator.
    std::uint32_t select_victim(std::uint32_t) override {
        return static_cast<std::uint32_t>(rng_() % ways_);  // remainder is < ways_, so it fits
    }

private:
    std::uint32_t ways_;
    std::mt19937_64 rng_;
};

}  // namespace

std::unique_ptr<ReplacementPolicy> make_random_policy(std::uint32_t ways, std::uint64_t seed) {
    return std::make_unique<RandomPolicy>(ways, seed);
}

}  // namespace ccsim
