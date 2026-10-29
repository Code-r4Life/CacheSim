// include/ccsim/analytics/metrics.hpp
//
// Everything the analytics view shows for ONE finished (or paused) run, derived from the
// engine's exact integer counters. Rates are std::optional<double>: nullopt when the
// denominator is 0, rendered as null / "n/a", never 0 or NaN (TECHNICAL_DESIGN 6).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ccsim/analytics/miss_classifier.hpp"
#include "ccsim/analytics/reuse.hpp"
#include "ccsim/sim/geometry.hpp"
#include "ccsim/sim/hierarchy.hpp"

namespace ccsim {

using Rate = std::optional<double>;

struct SetHeatmap {
    std::uint32_t sets = 0;
    std::uint32_t bin_size = 1;           // consecutive sets aggregated per bin (caps JSON size)
    std::vector<std::uint64_t> accesses;  // one entry per bin
    std::vector<std::uint64_t> misses;
    // Balance statistics over individual sets (not bins).
    std::uint32_t untouched_sets = 0;
    Rate access_cv;           // coefficient of variation of per-set accesses
    Rate max_to_mean_access;  // hottest set / mean set
};

struct LevelMetrics {
    std::string name;
    Geometry geometry;
    LevelStats stats;
    Rate hit_rate, miss_rate, read_miss_rate, write_miss_rate;
    Rate local_miss_rate, global_miss_rate;  // L2 only (L1: both equal miss_rate)
    Rate misses_per_kilo_access;             // per 1000 L1 accesses (traces carry no instructions)
    Rate eviction_rate;                      // evictions / demand accesses
    Rate dirty_eviction_ratio;               // writebacks / evictions
    std::optional<MissBreakdown> three_c;    // when classification was enabled
    SetHeatmap heatmap;
};

struct LatencyBreakdown {
    // Exact decomposition of total_latency_cycles (additive serial model):
    // l1 = t1 * L1.accesses, l2 = t2 * L2.accesses, memory = tmem * demand memory reads.
    std::uint64_t l1_cycles = 0, l2_cycles = 0, memory_cycles = 0;
};

struct HierarchyMetrics {
    Rate amat_cycles;           // exact: total / accesses
    Rate amat_textbook_cycles;  // t1 + m1 (t2 + m2 tmem): cross-check only
    LatencyBreakdown latency;
    Rate speedup_vs_memory;     // memory latency / AMAT: what the caches buy you
    std::uint64_t memory_read_bytes = 0, memory_write_bytes = 0;  // line-granular bus traffic
    Rate memory_bytes_per_access;
    Rate memory_accesses_per_kilo_access;
};

struct TimelinePoint {
    std::uint64_t accesses = 0;  // cumulative, at the end of this window
    std::uint64_t l1_hits = 0, l1_misses = 0;
    std::uint64_t l2_hits = 0, l2_misses = 0;
    std::uint64_t memory_reads = 0, memory_writes = 0;
    std::uint64_t latency_cycles = 0;
};

struct TraceInfo {
    std::string source;
    std::uint64_t accesses = 0, reads = 0, writes = 0;
    std::uint64_t fingerprint = 0;
};

struct RunReport {
    std::string label;
    HierarchyConfig config;
    TraceInfo trace;
    SimulationResult result;
    HierarchyMetrics metrics;
    std::vector<LevelMetrics> levels;  // L1, then L2 when present
    std::vector<TimelinePoint> timeline;
    std::optional<ReuseProfile> reuse;
    bool complete = false;  // false when built from a paused session
};

LevelMetrics level_metrics(const CacheLevel& level, const LevelStats& stats,
                           std::uint64_t l1_accesses, bool is_l2,
                           const std::optional<MissBreakdown>& three_c,
                           std::uint32_t max_heatmap_bins = 1024);

HierarchyMetrics hierarchy_metrics(const HierarchyConfig& config, const SimulationResult& result);

}  // namespace ccsim
