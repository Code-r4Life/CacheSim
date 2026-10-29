// Analytics: LineIndex, reuse distance, 3C classification, metrics, JSON.
// Correctness strategy: three independent implementations of "fully associative LRU"
// (the engine with a high-associativity cache, FullyAssociativeLru, and the reuse-distance
// profile) must agree exactly, and the reuse profile must equal a brute-force O(N^2) count.

#include <set>
#include <unordered_map>
#include <vector>

#include "ccsim/analytics/metrics.hpp"
#include "ccsim/analytics/miss_classifier.hpp"
#include "ccsim/analytics/reuse.hpp"
#include "ccsim/report/json.hpp"
#include "fixtures.hpp"
#include "harness.hpp"

using namespace ccsim;
using namespace ccsim::testing;

TEST_CASE("analytics.line_index_matches_unordered_map") {
    LineIndex idx(4);
    std::unordered_map<std::uint64_t, std::uint32_t> ref;
    std::uint64_t s = 12345;
    for (std::uint32_t i = 0; i < 200000; ++i) {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        const std::uint64_t key = (s >> 33) % 5000 * 64;  // strided keys stress the hash
        if ((s >> 20) % 3 == 0) {
            idx.erase(key);
            ref.erase(key);
        } else {
            idx.put(key, i);
            ref[key] = i;
        }
        if (i % 997 == 0) {
            CHECK_EQ(idx.size(), ref.size());
            for (const auto& [k, v] : ref) CHECK_EQ(idx.find(k), v);
        }
    }
    CHECK_EQ(idx.find(~0ULL - 1), LineIndex::kAbsent);
}

TEST_CASE("analytics.reuse_profile_matches_brute_force") {
    const auto t = synthetic_random(3000, 64 * 300, 0.0, 99);
    const ReuseProfile p = reuse_profile(*t, 64);
    std::vector<std::uint64_t> hist(1, 0);
    std::uint64_t cold = 0;
    for (std::size_t i = 0; i < t->size(); ++i) {
        const std::uint64_t line = t->address(i) >> 6;
        std::set<std::uint64_t> distinct;
        bool found = false;
        for (std::size_t j = i; j-- > 0;) {
            const std::uint64_t other = t->address(j) >> 6;
            if (other == line) {
                found = true;
                break;
            }
            distinct.insert(other);
        }
        if (!found) {
            ++cold;
            continue;
        }
        std::uint32_t b = 0;
        for (std::uint64_t d = distinct.size(); d != 0; d >>= 1) ++b;
        if (b >= hist.size()) hist.resize(b + 1, 0);
        ++hist[b];
    }
    CHECK_EQ(p.cold, cold);
    CHECK_EQ(p.histogram.size(), hist.size());
    for (std::size_t b = 0; b < hist.size() && b < p.histogram.size(); ++b) CHECK_EQ(p.histogram[b], hist[b]);
}

TEST_CASE("analytics.reuse_profile_survives_compaction") {
    // 400k accesses force timestamp compaction several times (capacity starts at 65536).
    // Cross-check against two independent FA-LRU models: the engine with a fully
    // associative cache (hash-index path for > 16 ways) and FullyAssociativeLru.
    const auto t = synthetic_random(400000, 64 * 3000, 0.0, 5);
    const ReuseProfile p = reuse_profile(*t, 64);
    for (std::uint32_t k : {4u, 7u, 10u, 11u, 12u}) {
        const std::uint64_t lines = 1ULL << k;
        const SimulationResult r = simulate(*t, l1_only(lines * 64, 64, static_cast<std::uint32_t>(lines), PolicyKind::Lru, 1, 100));
        FullyAssociativeLru fa(lines);
        std::uint64_t fa_misses = 0;
        for (std::size_t i = 0; i < t->size(); ++i) fa_misses += fa.access(t->address(i) >> 6) ? 0 : 1;
        CHECK_EQ(r.l1.misses, p.fa_lru_misses_pow2(k));
        CHECK_EQ(fa_misses, p.fa_lru_misses_pow2(k));
    }
    CHECK_EQ(p.cold, 3000u);
}

TEST_CASE("analytics.three_c_classification") {
    // Direct-mapped 2-set cache (16 B lines), capacity 2 lines.
    // Lines 0, 2, 0, 2 all map to set 0: first two misses compulsory, then conflict misses
    // (a 2-line FA-LRU would hit). Then lines 4, 6, 8, 0: 4,6,8 compulsory, 0 capacity.
    const auto t = trace_from_text("R 0x00\nR 0x20\nR 0x00\nR 0x20\nR 0x40\nR 0x60\nR 0x80\nR 0x00\n");
    Session s(t, l1_only(32, 16, 1, PolicyKind::Lru, 1, 100), kRef);
    s.run_to_end();
    const MissBreakdown b = *s.three_c(LevelId::L1);
    CHECK_EQ(b.compulsory, 5u);
    CHECK_EQ(b.conflict, 2u);
    CHECK_EQ(b.capacity, 1u);
    CHECK_EQ(b.compulsory + b.conflict + b.capacity, s.result().l1.misses);
}

TEST_CASE("analytics.metrics_and_null_rates") {
    const auto t = trace_from_text("R 0x00\nR 0x00\n");
    Session s(t, default_hierarchy(), kRef);
    s.run_to_end();
    const RunReport r = s.report("x");
    CHECK_NEAR(*r.levels[0].miss_rate, 0.5, 1e-12);
    CHECK(!r.levels[0].write_miss_rate.has_value());     // no writes: n/a, never 0
    CHECK(!r.levels[0].dirty_eviction_ratio.has_value());  // no evictions
    CHECK_NEAR(*r.levels[1].global_miss_rate, 0.5, 1e-12);
    // Exact latency breakdown sums to the total.
    const LatencyBreakdown& lb = r.metrics.latency;
    CHECK_EQ(lb.l1_cycles + lb.l2_cycles + lb.memory_cycles, r.result.total_latency_cycles);
    CHECK_NEAR(*r.metrics.amat_cycles, *r.metrics.amat_textbook_cycles, 1e-9);
    CHECK_EQ(r.levels[0].heatmap.sets, 64u);
    CHECK_EQ(r.levels[0].heatmap.untouched_sets, 63u);
}

TEST_CASE("analytics.heatmap_bins_cap_size") {
    HierarchyConfig c = l1_only(1U << 20, 64, 1, PolicyKind::Lru, 1, 100);  // 16384 sets
    const auto t = synthetic_stride(50000, 64, 1U << 22);
    Session s(t, c, kRef);
    s.run_to_end();
    const RunReport r = s.report("bins");
    CHECK_EQ(r.levels[0].heatmap.sets, 16384u);
    CHECK_EQ(r.levels[0].heatmap.bin_size, 16u);
    CHECK_EQ(r.levels[0].heatmap.accesses.size(), std::size_t{1024});
    std::uint64_t total = 0;
    for (const auto v : r.levels[0].heatmap.accesses) total += v;
    CHECK_EQ(total, 50000u);
}

TEST_CASE("report.json_shape_and_determinism") {
    const auto t = load_trace_file(data_dir() + "/golden/demo_b_two_level.trace");
    HierarchyConfig c;
    c.l1 = level("L1", 64, 16, 2, PolicyKind::Lru, 1);
    c.l2 = level("L2", 128, 16, 2, PolicyKind::Lru, 10);
    c.memory_latency_cycles = 100;
    RunOptions o;
    o.label = "demo-b";
    o.reuse_profile = true;
    const std::string a = report_to_json(run_batch(t, c, kRef, o), "test");
    const std::string b = report_to_json(run_batch(t, c, kRef, o), "test");
    CHECK(a == b);
    CHECK(a.find("\"schema\":\"ccsim.simulation/1\"") != std::string::npos);
    CHECK(a.find("\"amat_cycles\":68.0") != std::string::npos);
    CHECK(a.find("\"total_latency_cycles\":680") != std::string::npos);

    JsonWriter w;
    w.begin_object().field("s", "a\"b\\c\n").field("n", Rate{}).key("h").hex(255).end_object();
    CHECK_EQ(w.str(), std::string("{\"s\":\"a\\\"b\\\\c\\n\",\"n\":null,\"h\":\"0xff\"}"));
}

TEST_CASE("report.event_json") {
    const auto t = trace_from_text("W 0x1234\n");
    Session s(t, default_hierarchy(), kRef);
    std::vector<AccessEvent> ev;
    s.step(1, &ev);
    const std::string j = event_to_json(ev[0]);
    CHECK(j.find("\"schema\":\"ccsim.events/1\"") != std::string::npos);
    CHECK(j.find("\"address\":\"0x1234\"") != std::string::npos);
    CHECK(j.find("\"kind\":\"memory_read\"") != std::string::npos);
    CHECK(j.find("\"latency_cycles\":216") != std::string::npos);
}
