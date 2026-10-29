// Golden results: Demos A-C of docs/WORKING_DEMO.md section 2. Every expected number was
// derived by hand / an independent reference model in that document, NOT copied from this
// implementation's output (CONTRIBUTING section 8).
//
// Each check runs once per policy factory: always with the test stand-ins, and also with
// the production make_policy once src/policy/ exists, so the policy owner's code has to
// reproduce the same integers before it can merge.

#include <string>
#include <vector>

#include "fixtures.hpp"
#include "harness.hpp"

using namespace ccsim;
using namespace ccsim::testing;

namespace {

struct Factory {
    const char* name;
    PolicyFactory make;
};

std::vector<Factory> factories() {
    std::vector<Factory> f{{"reference", kRef}};
#ifdef CCSIM_HAS_PRODUCTION_POLICIES
    f.push_back({"production", &make_policy});
#endif
    return f;
}

std::shared_ptr<Trace> golden(const char* file) { return load_trace_file(data_dir() + "/golden/" + file); }

}  // namespace

TEST_CASE("golden.demo_a_conflict_and_associativity") {
    const auto t = golden("demo_a_conflict.trace");
    CHECK_EQ(t->size(), std::size_t{6});
    struct Row { std::uint32_t ways, sets; std::uint64_t hits, misses, evictions, mem_reads, total; };
    const Row rows[] = {{1, 4, 0, 6, 5, 6, 606}, {2, 2, 4, 2, 0, 2, 206}, {4, 1, 4, 2, 0, 2, 206}};
    for (const Factory& f : factories()) {
        for (const Row& row : rows) {
            const HierarchyConfig c = l1_only(64, 16, row.ways, PolicyKind::Lru, 1, 100);
            Hierarchy h(c, f.make);
            CHECK_EQ(h.l1().geometry().sets, row.sets);
            for (std::size_t i = 0; i < t->size(); ++i) h.access((*t)[i]);
            const SimulationResult& r = h.result();
            CHECK_EQ(r.l1.hits, row.hits);
            CHECK_EQ(r.l1.misses, row.misses);
            CHECK_EQ(r.l1.evictions, row.evictions);
            CHECK_EQ(r.memory.reads, row.mem_reads);
            CHECK_EQ(r.total_latency_cycles, row.total);
            CHECK_NEAR(*r.amat_cycles(), static_cast<double>(row.total) / 6.0, 1e-9);
            check_invariants(r, c, std::string("demo A ") + f.name);
        }
    }
}

TEST_CASE("golden.demo_a_random_any_seed") {
    // Invalid ways fill first, so 2-way Random never consults the policy here.
    const auto t = golden("demo_a_conflict.trace");
    for (const Factory& f : factories()) {
        for (std::uint64_t seed : {1ULL, 2ULL, 42ULL, 0xdeadbeefULL, ~0ULL}) {
            HierarchyConfig c = l1_only(64, 16, 2, PolicyKind::Random, 1, 100);
            c.seed = seed;
            const SimulationResult r = simulate(*t, c, f.make);
            CHECK_EQ(r.l1.hits, 4u);
            CHECK_EQ(r.l1.misses, 2u);
        }
    }
}

TEST_CASE("golden.demo_b_two_level_every_step") {
    const auto t = golden("demo_b_two_level.trace");
    HierarchyConfig c;
    c.l1 = level("L1", 64, 16, 2, PolicyKind::Lru, 1);
    c.l2 = level("L2", 128, 16, 2, PolicyKind::Lru, 10);
    c.memory_latency_cycles = 100;

    const std::uint32_t latency[] = {111, 1, 111, 111, 11, 111, 111, 111, 1, 1};
    const Outcome l1[] = {Outcome::Miss, Outcome::Hit, Outcome::Miss, Outcome::Miss, Outcome::Miss,
                          Outcome::Miss, Outcome::Miss, Outcome::Miss, Outcome::Hit, Outcome::Hit};
    const Outcome l2[] = {Outcome::Miss, Outcome::NotAccessed, Outcome::Miss, Outcome::Miss, Outcome::Hit,
                          Outcome::Miss, Outcome::Miss, Outcome::Miss, Outcome::NotAccessed, Outcome::NotAccessed};
    const std::uint64_t mem_r[] = {1, 0, 1, 1, 0, 1, 1, 1, 0, 0};
    const std::uint64_t mem_w[] = {0, 0, 0, 0, 0, 0, 1, 0, 0, 0};

    for (const Factory& f : factories()) {
        Hierarchy h(c, f.make);
        std::uint64_t prev_r = 0, prev_w = 0;
        for (std::size_t i = 0; i < t->size(); ++i) {
            AccessEvent e;
            h.step((*t)[i], &e);
            CHECK_EQ(e.seq, i);
            CHECK_EQ(e.latency_cycles, latency[i]);
            CHECK(e.l1.outcome == l1[i]);
            CHECK(e.l2.outcome == l2[i]);
            CHECK_EQ(h.result().memory.reads - prev_r, mem_r[i]);
            CHECK_EQ(h.result().memory.writes - prev_w, mem_w[i]);
            prev_r = h.result().memory.reads;
            prev_w = h.result().memory.writes;

            if (i == 3) {
                // Step 4 (R 0x080): the dirty L1 victim (line 0) is written into L2 BEFORE the
                // demand fetch and refreshes its LRU position, so L2 evicts line 4 instead.
                const ActionKind expect[] = {ActionKind::Lookup, ActionKind::Evict, ActionKind::Writeback,
                                             ActionKind::Lookup, ActionKind::Evict, ActionKind::MemoryRead,
                                             ActionKind::Fill, ActionKind::Fill};
                CHECK_EQ(static_cast<int>(e.action_count), 8);
                for (std::size_t k = 0; k < 8 && k < e.action_count; ++k) CHECK(e.actions[k].kind == expect[k]);
                CHECK_EQ(e.actions[1].line, 0u);                                    // L1 victim: line 0
                CHECK((e.actions[1].flags & Action::kDirty) != 0);                  // ... dirty
                CHECK((e.actions[2].flags & Action::kHit) != 0);                    // write-back hits L2
                CHECK_EQ(e.actions[4].line, 4u);                                    // L2 victim: line 4
                CHECK_EQ(e.actions[4].way, 1u);
                CHECK_EQ(e.l1.tag, 4u);  // decompose: L1 tag 4, set 0
                CHECK_EQ(e.l1.set, 0u);
                CHECK_EQ(e.l2.tag, 2u);  // L2 tag 2, set 0
                CHECK_EQ(e.l2.way, 1u);
            }
        }
        const SimulationResult& r = h.result();
        CHECK_EQ(r.l1.accesses, 10u);
        CHECK_EQ(r.l1.reads, 8u);
        CHECK_EQ(r.l1.writes, 2u);
        CHECK_EQ(r.l1.hits, 3u);
        CHECK_EQ(r.l1.misses, 7u);
        CHECK_EQ(r.l1.read_misses, 6u);
        CHECK_EQ(r.l1.write_misses, 1u);
        CHECK_EQ(r.l1.evictions, 4u);
        CHECK_EQ(r.l1.writebacks, 1u);
        CHECK_EQ(r.l2->accesses, 7u);
        CHECK_EQ(r.l2->hits, 1u);
        CHECK_EQ(r.l2->misses, 6u);
        CHECK_EQ(r.l2->evictions, 3u);
        CHECK_EQ(r.l2->writebacks, 1u);
        CHECK_EQ(r.l2->writebacks_received, 1u);
        CHECK_EQ(r.l2->writeback_hits, 1u);
        CHECK_EQ(r.l2->writeback_misses, 0u);
        CHECK_EQ(r.memory.reads, 6u);
        CHECK_EQ(r.memory.writes, 1u);
        CHECK_EQ(r.total_latency_cycles, 680u);
        CHECK(*r.amat_cycles() == 68.0);  // exact, not approximately (TECHNICAL_DESIGN 6)
        check_invariants(r, c, std::string("demo B ") + f.name);
    }
}

TEST_CASE("golden.demo_c_belady_anomaly") {
    const auto t = golden("demo_c_belady.trace");
    struct Row { PolicyKind policy; std::uint32_t ways; std::uint64_t hits, misses, evictions; };
    const Row rows[] = {{PolicyKind::Fifo, 3, 3, 9, 6}, {PolicyKind::Fifo, 4, 2, 10, 6},
                        {PolicyKind::Lru, 3, 2, 10, 7}, {PolicyKind::Lru, 4, 4, 8, 4}};
    for (const Factory& f : factories()) {
        for (const Row& row : rows) {
            const HierarchyConfig c = l1_only(16ULL * row.ways, 16, row.ways, row.policy, 1, 100);
            const SimulationResult r = simulate(*t, c, f.make);
            CHECK_EQ(r.l1.hits, row.hits);
            CHECK_EQ(r.l1.misses, row.misses);
            CHECK_EQ(r.l1.evictions, row.evictions);
            check_invariants(r, c, std::string("demo C ") + f.name);
        }
    }
}
