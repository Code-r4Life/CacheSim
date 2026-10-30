// Contract extension 1.1 semantics (docs/ARCHITECTURE.md section 5.3): inclusion,
// write-through, no-write-allocate, plus config rejection. Every expected value is derived
// by hand in the comment above it. Latencies: t1 = 1, t2 = 10, memory = 100.

#include "fixtures.hpp"
#include "harness.hpp"

using namespace ccsim;
using namespace ccsim::testing;

namespace {

// L1: 32 B, 16 B lines, 2-way -> 1 set (fully associative, 2 lines).
// L2: 32 B, 16 B lines, 1-way -> 2 sets (direct-mapped). Lines 0 and 2 share L2 set 0.
HierarchyConfig inclusion_config(InclusionPolicy p) {
    HierarchyConfig c;
    c.l1 = level("L1", 32, 16, 2, PolicyKind::Lru, 1);
    c.l2 = level("L2", 32, 16, 1, PolicyKind::Lru, 10);
    c.memory_latency_cycles = 100;
    c.inclusion = p;
    return c;
}

// L1: one 16 B line. L2: 32 B, 2-way -> one set of 2 lines.
HierarchyConfig exclusive_config(InclusionPolicy p) {
    HierarchyConfig c;
    c.l1 = level("L1", 16, 16, 1, PolicyKind::Lru, 1);
    c.l2 = level("L2", 32, 16, 2, PolicyKind::Lru, 10);
    c.memory_latency_cycles = 100;
    c.inclusion = p;
    return c;
}

}  // namespace

TEST_CASE("sim.inclusive_back_invalidates_l1") {
    // A=0x00 (line 0), B=0x20 (line 2), both L2 set 0. Trace A, B, A.
    // NINE: B evicts A from L2 only; the final A hits in L1.
    // Inclusive: B's L2 fill evicts A and back-invalidates it in L1, so the final A misses
    // everywhere and in turn evicts B from L2, back-invalidating B in L1.
    const auto t = trace_from_text("R 0x00\nR 0x20\nR 0x00\n");
    const SimulationResult nine = simulate(*t, inclusion_config(InclusionPolicy::NonInclusive));
    CHECK_EQ(nine.l1.hits, 1u);
    CHECK_EQ(nine.l1.misses, 2u);
    CHECK_EQ(nine.l1.back_invalidations, 0u);
    CHECK_EQ(nine.memory.reads, 2u);

    const HierarchyConfig ic = inclusion_config(InclusionPolicy::Inclusive);
    const SimulationResult inc = simulate(*t, ic);
    CHECK_EQ(inc.l1.hits, 0u);
    CHECK_EQ(inc.l1.misses, 3u);
    CHECK_EQ(inc.l1.evictions, 0u);  // back-invalidations are not replacement evictions
    CHECK_EQ(inc.l1.back_invalidations, 2u);
    CHECK_EQ(inc.l2->misses, 3u);
    CHECK_EQ(inc.l2->evictions, 2u);
    CHECK_EQ(inc.memory.reads, 3u);
    CHECK_EQ(inc.total_latency_cycles, 333u);
    check_invariants(inc, ic, "inclusive A,B,A");
}

TEST_CASE("sim.inclusive_dirty_back_invalidation_writes_memory_once") {
    // W A dirties A in L1 (L2 copy clean). R B evicts A from L2; the dirty L1 copy is merged
    // into that eviction: exactly one memory write, counted as an L2 write-back.
    const auto t = trace_from_text("W 0x00\nR 0x20\n");
    const HierarchyConfig ic = inclusion_config(InclusionPolicy::Inclusive);
    Hierarchy h(ic, kRef);
    AccessEvent e;
    h.step((*t)[0], nullptr);
    h.step((*t)[1], &e);
    const SimulationResult& r = h.result();
    CHECK_EQ(r.l1.back_invalidations, 1u);
    CHECK_EQ(r.l1.writebacks, 0u);
    CHECK_EQ(r.l2->writebacks, 1u);
    CHECK_EQ(r.memory.writes, 1u);
    CHECK_EQ(r.memory.reads, 2u);
    // Order: L1 lookup, L2 lookup, L2 evict A, back-invalidate A (dirty), memory write,
    // memory read, L2 fill B, L1 fill B.
    const ActionKind expect[] = {ActionKind::Lookup, ActionKind::Lookup, ActionKind::Evict, ActionKind::BackInvalidate,
                                 ActionKind::MemoryWrite, ActionKind::MemoryRead, ActionKind::Fill, ActionKind::Fill};
    CHECK_EQ(static_cast<int>(e.action_count), 8);
    for (std::size_t k = 0; k < 8 && k < e.action_count; ++k) CHECK(e.actions[k].kind == expect[k]);
    CHECK((e.actions[3].flags & Action::kDirty) != 0);
    check_invariants(r, ic, "inclusive dirty");
}

TEST_CASE("sim.exclusive_extends_capacity") {
    // Trace A, B, C, A (lines 0, 1, 2). Exclusive keeps 1 line in L1 + 2 victims in L2,
    // so the final A hits in L2. NINE's L2 holds only A, B then C evicts A (LRU).
    const auto t = trace_from_text("R 0x00\nR 0x10\nR 0x20\nR 0x00\n");
    const SimulationResult nine = simulate(*t, exclusive_config(InclusionPolicy::NonInclusive));
    CHECK_EQ(nine.l2->hits, 0u);
    CHECK_EQ(nine.memory.reads, 4u);

    const HierarchyConfig xc = exclusive_config(InclusionPolicy::Exclusive);
    const SimulationResult ex = simulate(*t, xc);
    CHECK_EQ(ex.l1.misses, 4u);
    CHECK_EQ(ex.l1.evictions, 3u);
    CHECK_EQ(ex.l2->accesses, 4u);
    CHECK_EQ(ex.l2->hits, 1u);
    CHECK_EQ(ex.l2->misses, 3u);
    CHECK_EQ(ex.l2->victims_received, 3u);
    CHECK_EQ(ex.l2->evictions, 0u);  // A's move-out freed the slot C's victim insert used
    CHECK_EQ(ex.memory.reads, 3u);
    CHECK_EQ(ex.total_latency_cycles, 111u * 3 + 11u);
    check_invariants(ex, xc, "exclusive A,B,C,A");
}

TEST_CASE("sim.exclusive_preserves_dirtiness_through_moves") {
    // W A (A dirty in L1), R B (dirty A moves to L2), R A (dirty A moves back up, B down),
    // R B (dirty A moves down again). Nothing ever reaches memory as a write.
    const auto t = trace_from_text("W 0x00\nR 0x10\nR 0x00\nR 0x10\n");
    const HierarchyConfig xc = exclusive_config(InclusionPolicy::Exclusive);
    const SimulationResult r = simulate(*t, xc);
    CHECK_EQ(r.l1.writebacks, 2u);
    CHECK_EQ(r.l2->writebacks_received, 2u);
    CHECK_EQ(r.l2->writeback_misses, 2u);
    CHECK_EQ(r.memory.reads, 2u);
    CHECK_EQ(r.memory.writes, 0u);
    check_invariants(r, xc, "exclusive dirty");
}

TEST_CASE("sim.write_through_l1_keeps_lines_clean") {
    // L1 32 B 2-way (1 set), L2 64 B 2-way. W A, W A, R A.
    // Each store is forwarded to L2 (which, write-back, absorbs it as dirty).
    HierarchyConfig c;
    c.l1 = level("L1", 32, 16, 2, PolicyKind::Lru, 1);
    c.l1.write_policy = WritePolicy::WriteThrough;
    c.l2 = level("L2", 64, 16, 2, PolicyKind::Lru, 10);
    c.memory_latency_cycles = 100;
    const auto t = trace_from_text("W 0x00\nW 0x04\nR 0x08\n");
    Hierarchy h(c, kRef);
    for (std::size_t i = 0; i < t->size(); ++i) h.access((*t)[i]);
    const SimulationResult& r = h.result();
    CHECK_EQ(r.l1.hits, 2u);
    CHECK_EQ(r.l1.misses, 1u);
    CHECK_EQ(r.l1.write_throughs_sent, 2u);
    CHECK_EQ(r.l2->write_throughs_received, 2u);
    CHECK_EQ(r.memory.reads, 1u);
    CHECK_EQ(r.memory.writes, 0u);
    CHECK(!h.l1().is_dirty(0, 0));
    CHECK(h.l2()->is_dirty(0, 0));
    check_invariants(r, c, "write-through L1");

    // Write-through at both levels: every store reaches memory.
    c.l2->write_policy = WritePolicy::WriteThrough;
    const SimulationResult r2 = simulate(*t, c);
    CHECK_EQ(r2.memory.writes, 2u);
    CHECK_EQ(r2.l2->write_throughs_sent, 2u);
    check_invariants(r2, c, "write-through both");
}

TEST_CASE("sim.no_write_allocate_writes_around") {
    // L1-only, WB + no-write-allocate. W A (miss, forwarded, not installed, costs t1),
    // R A (miss, allocates), W A (hit, dirties).
    HierarchyConfig c = l1_only(32, 16, 2, PolicyKind::Lru, 1, 100);
    c.l1.write_miss_policy = WriteMissPolicy::NoWriteAllocate;
    const auto t = trace_from_text("W 0x00\nR 0x00\nW 0x00\n");
    const SimulationResult r = simulate(*t, c);
    CHECK_EQ(r.l1.misses, 2u);
    CHECK_EQ(r.l1.write_misses, 1u);
    CHECK_EQ(r.l1.no_allocate_misses, 1u);
    CHECK_EQ(r.l1.hits, 1u);
    CHECK_EQ(r.memory.reads, 1u);
    CHECK_EQ(r.memory.writes, 1u);
    CHECK_EQ(r.total_latency_cycles, 103u);
    check_invariants(r, c, "nwa L1-only");

    // With L2: the forwarded store misses L2 and goes around it too (partial stores never allocate).
    HierarchyConfig c2 = c;
    c2.l2 = level("L2", 64, 16, 2, PolicyKind::Lru, 10);
    const SimulationResult r2 = simulate(*t, c2);
    CHECK_EQ(r2.l2->accesses, 1u);
    CHECK_EQ(r2.l2->write_throughs_received, 1u);
    CHECK_EQ(r2.l2->write_throughs_sent, 1u);
    CHECK_EQ(r2.memory.writes, 1u);
    check_invariants(r2, c2, "nwa with L2");
}

TEST_CASE("sim.rejects_invalid_geometry") {
    auto make = [](HierarchyConfig c) { return Hierarchy(c, kRef); };
    CHECK_THROWS_AS(make(l1_only(64, 24, 1, PolicyKind::Lru, 1, 100)), ConfigError);  // line not 2^k
    CHECK_THROWS_AS(make(l1_only(96, 16, 2, PolicyKind::Lru, 1, 100)), ConfigError);  // 3 sets
    CHECK_THROWS_AS(make(l1_only(64, 16, 0, PolicyKind::Lru, 1, 100)), ConfigError);  // 0 ways
    CHECK_THROWS_AS(make(l1_only(64, 16, 1, PolicyKind::Lru, 0, 100)), ConfigError);  // latency 0
    CHECK_THROWS_AS(make(l1_only(64, 16, 1, PolicyKind::Lru, 1, 0)), ConfigError);    // memory latency 0
    HierarchyConfig c = default_hierarchy();
    c.l2->line_bytes = 128;
    CHECK_THROWS_AS(make(c), ConfigError);  // unequal line sizes
    // 48 KiB, 12-way, 64 B lines = 64 sets: valid (only the set count must be 2^k).
    CHECK_EQ(Hierarchy(l1_only(48 * 1024, 64, 12, PolicyKind::Lru, 1, 100), kRef).l1().geometry().sets, 64u);
}

TEST_CASE("sim.address_mapping") {
    // TECHNICAL_DESIGN section 4 example: 64 B, 2-way, 16 B lines -> 2 sets;
    // 0x0C4 is line 12, set 0, tag 6.
    const Geometry g = Geometry::from(level("L1", 64, 16, 2, PolicyKind::Lru, 1));
    CHECK_EQ(g.sets, 2u);
    CHECK_EQ(g.offset_bits, 4u);
    CHECK_EQ(g.index_bits, 1u);
    CHECK_EQ(g.line_of(0x0C4), 12u);
    CHECK_EQ(g.set_of(12), 0u);
    CHECK_EQ(g.tag_of(12), 6u);
    CHECK_EQ(g.offset_of(0x0C4), 4u);
    CHECK_EQ(g.line_from(6, 0), 12u);
    // Default L1 example from OVERVIEW: 0x7ffd1b64 lands in set 45.
    const Geometry d = Geometry::from(level("L1", 32 * 1024, 64, 8, PolicyKind::Lru, 4));
    CHECK_EQ(d.set_of(d.line_of(0x7ffd1b64)), 45u);
}

TEST_CASE("sim.high_associativity_index_matches_linear_scan") {
    // ways <= 16 uses the linear scan, ways > 16 the hash index. With a working set of
    // exactly 16 lines both fully associative caches must take only compulsory misses.
    // (The 3-way cross-check against FA-LRU and the reuse profile is in test_analytics.)
    const auto t = synthetic_random(20000, 16 * 64, 0.3, 7);  // 16 distinct 64 B lines
    const SimulationResult a = simulate(*t, l1_only(16 * 64, 64, 16, PolicyKind::Lru, 1, 100));  // linear
    const SimulationResult b = simulate(*t, l1_only(32 * 64, 64, 32, PolicyKind::Lru, 1, 100));  // index
    CHECK_EQ(a.l1.misses, 16u);  // only compulsory misses: everything fits in both
    CHECK_EQ(b.l1.misses, 16u);
    CHECK_EQ(a.l1.hits, b.l1.hits);
}
