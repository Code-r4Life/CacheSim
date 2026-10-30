// Production replacement policies (src/policy/): LRU, FIFO, Random and the factory.
//
// These tests drive the policies directly through the ReplacementPolicy interface, with no
// engine involved. Expected numbers are hand-derived (WORKING_DEMO Demo C, plus the small
// examples below), never copied from this implementation's output.

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

#include "ccsim/policy/replacement_policy.hpp"
#include "harness.hpp"

using namespace ccsim;
using namespace ccsim::testing;

namespace {

struct Counts {
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    std::uint64_t evictions = 0;
};

// A tiny fully associative cache (one set) that talks to the policy exactly the way the
// engine does: hit -> on_hit; miss -> lowest empty way, else select_victim; then on_fill.
Counts run_fully_associative(PolicyKind kind, std::uint32_t lines,
                             const std::vector<std::uint64_t>& refs) {
    auto policy = make_policy(kind, 1, lines, 1);
    std::vector<std::uint64_t> tag(lines, 0);
    std::vector<bool> valid(lines, false);
    Counts c;
    for (const std::uint64_t ref : refs) {
        bool hit = false;
        for (std::uint32_t w = 0; w < lines; ++w) {
            if (valid[w] && tag[w] == ref) {
                policy->on_hit(0, w);
                hit = true;
                break;
            }
        }
        if (hit) {
            ++c.hits;
            continue;
        }
        ++c.misses;
        std::uint32_t way = lines;  // "no empty way found yet"
        for (std::uint32_t w = 0; w < lines; ++w) {
            if (!valid[w]) {
                way = w;
                break;
            }
        }
        if (way == lines) {
            way = policy->select_victim(0);
            ++c.evictions;
        }
        tag[way] = ref;
        valid[way] = true;
        policy->on_fill(0, way);
    }
    return c;
}

// Belady's reference string from WORKING_DEMO Demo C.
const std::vector<std::uint64_t> kBelady = {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5};

}  // namespace

TEST_CASE("policy.demo_c_fifo_has_beladys_anomaly") {
    const Counts three = run_fully_associative(PolicyKind::Fifo, 3, kBelady);
    CHECK_EQ(three.hits, 3u);
    CHECK_EQ(three.misses, 9u);
    CHECK_EQ(three.evictions, 6u);
    const Counts four = run_fully_associative(PolicyKind::Fifo, 4, kBelady);
    CHECK_EQ(four.hits, 2u);
    CHECK_EQ(four.misses, 10u);
    CHECK_EQ(four.evictions, 6u);
    CHECK(four.misses > three.misses);  // more lines, MORE misses: the anomaly
}

TEST_CASE("policy.demo_c_lru_has_no_anomaly") {
    const Counts three = run_fully_associative(PolicyKind::Lru, 3, kBelady);
    CHECK_EQ(three.hits, 2u);
    CHECK_EQ(three.misses, 10u);
    CHECK_EQ(three.evictions, 7u);
    const Counts four = run_fully_associative(PolicyKind::Lru, 4, kBelady);
    CHECK_EQ(four.hits, 4u);
    CHECK_EQ(four.misses, 8u);
    CHECK_EQ(four.evictions, 4u);
    CHECK(four.misses <= three.misses);
}

TEST_CASE("policy.lru_and_fifo_differ_after_a_reuse") {
    // 2-way set: A fills way 0, B fills way 1, then A is used again (hit on way 0).
    auto lru = make_policy(PolicyKind::Lru, 1, 2, 1);
    auto fifo = make_policy(PolicyKind::Fifo, 1, 2, 1);
    for (auto* p : {lru.get(), fifo.get()}) {
        p->on_fill(0, 0);
        p->on_fill(0, 1);
        p->on_hit(0, 0);
    }
    CHECK_EQ(lru->select_victim(0), 1u);   // LRU: B was used longest ago
    CHECK_EQ(fifo->select_victim(0), 0u);  // FIFO: A arrived first, the hit does not matter
}

TEST_CASE("policy.sets_are_independent") {
    auto lru = make_policy(PolicyKind::Lru, 2, 2, 1);
    lru->on_fill(0, 0);
    lru->on_fill(0, 1);
    lru->on_fill(1, 1);  // set 1 is filled in the opposite order
    lru->on_fill(1, 0);
    lru->on_hit(0, 0);   // touching set 0 must not disturb set 1
    CHECK_EQ(lru->select_victim(0), 1u);
    CHECK_EQ(lru->select_victim(1), 1u);
}

TEST_CASE("policy.refilled_slot_becomes_the_newest") {
    // The engine can remove a line silently and later refill the same slot. on_fill must
    // overwrite the old state: way 0 is refilled last, so way 1 is now the oldest.
    for (const PolicyKind kind : {PolicyKind::Lru, PolicyKind::Fifo}) {
        auto p = make_policy(kind, 1, 3, 1);
        p->on_fill(0, 0);
        p->on_fill(0, 1);
        p->on_fill(0, 2);
        p->on_fill(0, 0);
        CHECK_EQ(p->select_victim(0), 1u);
    }
}

TEST_CASE("policy.one_way_always_picks_way_zero") {
    for (std::size_t k = 0; k < kPolicyKindCount; ++k) {
        auto p = make_policy(static_cast<PolicyKind>(k), 4, 1, 7);
        p->on_fill(2, 0);
        CHECK_EQ(p->select_victim(2), 0u);
    }
}

TEST_CASE("policy.random_follows_mt19937_64_modulo_exactly") {
    // The recipe is part of the contract: mt19937_64 seeded with `seed`, victim = rng() % ways,
    // and the generator advances only inside select_victim.
    auto p = make_policy(PolicyKind::Random, 8, 5, 42);
    std::mt19937_64 oracle(42);
    for (int i = 0; i < 100; ++i) {
        p->on_hit(1, 2);   // must not advance the generator
        p->on_fill(3, 4);  // neither must this
        CHECK_EQ(p->select_victim(static_cast<std::uint32_t>(i % 8)),
                 static_cast<std::uint32_t>(oracle() % 5));
    }
}

TEST_CASE("policy.random_is_repeatable_and_seed_dependent") {
    auto a = make_policy(PolicyKind::Random, 1, 8, 1);
    auto b = make_policy(PolicyKind::Random, 1, 8, 1);
    auto c = make_policy(PolicyKind::Random, 1, 8, 2);
    bool differs = false;
    for (int i = 0; i < 200; ++i) {
        const std::uint32_t va = a->select_victim(0);
        CHECK_EQ(va, b->select_victim(0));
        if (va != c->select_victim(0)) differs = true;
    }
    CHECK(differs);  // seeds 1 and 2 agreeing 200 times in a row is (1/8)^200: impossible in practice
}

TEST_CASE("policy.random_stays_in_range_and_uses_every_way") {
    auto p = make_policy(PolicyKind::Random, 1, 3, 9);
    std::vector<int> seen(3, 0);
    for (int i = 0; i < 600; ++i) {
        const std::uint32_t v = p->select_victim(0);
        CHECK(v < 3u);
        if (v < 3u) ++seen[v];
    }
    for (const int n : seen) CHECK(n > 0);
}

TEST_CASE("policy.factory_builds_every_registered_kind") {
    for (std::size_t k = 0; k < kPolicyKindCount; ++k) {
        auto p = make_policy(static_cast<PolicyKind>(k), 4, 4, 1);
        CHECK(p != nullptr);
        if (p) {
            for (std::uint32_t w = 0; w < 4; ++w) p->on_fill(3, w);
            CHECK(p->select_victim(3) < 4u);
        }
    }
}

TEST_CASE("policy.factory_rejects_bad_arguments") {
    CHECK_THROWS_AS(make_policy(PolicyKind::Lru, 0, 4, 1), ConfigError);
    CHECK_THROWS_AS(make_policy(PolicyKind::Fifo, 4, 0, 1), ConfigError);
    CHECK_THROWS_AS(make_policy(static_cast<PolicyKind>(99), 4, 4, 1), ConfigError);
}
