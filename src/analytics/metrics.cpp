#include "ccsim/analytics/metrics.hpp"

#include <cmath>

namespace ccsim {
namespace {

Rate ratio(std::uint64_t num, std::uint64_t den) {
    if (den == 0) return std::nullopt;
    return static_cast<double>(num) / static_cast<double>(den);
}

SetHeatmap make_heatmap(const CacheLevel& level, std::uint32_t max_bins) {
    const auto& acc = level.set_accesses();
    const auto& mis = level.set_misses();
    SetHeatmap h;
    h.sets = static_cast<std::uint32_t>(acc.size());
    if (h.sets == 0) return h;
    if (max_bins == 0) max_bins = 1;
    h.bin_size = (h.sets + max_bins - 1) / max_bins;
    const std::uint32_t bins = (h.sets + h.bin_size - 1) / h.bin_size;
    h.accesses.assign(bins, 0);
    h.misses.assign(bins, 0);

    double sum = 0, sum_sq = 0;
    std::uint64_t peak = 0;
    for (std::uint32_t s = 0; s < h.sets; ++s) {
        h.accesses[s / h.bin_size] += acc[s];
        h.misses[s / h.bin_size] += mis[s];
        if (acc[s] == 0) ++h.untouched_sets;
        if (acc[s] > peak) peak = acc[s];
        const double v = static_cast<double>(acc[s]);
        sum += v;
        sum_sq += v * v;
    }
    const double mean = sum / h.sets;
    if (mean > 0) {
        const double var = std::max(0.0, sum_sq / h.sets - mean * mean);
        h.access_cv = std::sqrt(var) / mean;
        h.max_to_mean_access = static_cast<double>(peak) / mean;
    }
    return h;
}

}  // namespace

LevelMetrics level_metrics(const CacheLevel& level, const LevelStats& st, std::uint64_t l1_accesses,
                           bool is_l2, const std::optional<MissBreakdown>& three_c,
                           std::uint32_t max_heatmap_bins) {
    LevelMetrics m;
    m.name = level.config().name;
    m.geometry = level.geometry();
    m.stats = st;
    m.hit_rate = ratio(st.hits, st.accesses);
    m.miss_rate = ratio(st.misses, st.accesses);
    m.read_miss_rate = ratio(st.read_misses, st.reads);
    m.write_miss_rate = ratio(st.write_misses, st.writes);
    m.local_miss_rate = m.miss_rate;
    m.global_miss_rate = is_l2 ? ratio(st.misses, l1_accesses) : m.miss_rate;
    if (l1_accesses != 0) m.misses_per_kilo_access = 1000.0 * static_cast<double>(st.misses) / static_cast<double>(l1_accesses);
    m.eviction_rate = ratio(st.evictions, st.accesses);
    m.dirty_eviction_ratio = ratio(st.writebacks, st.evictions);
    m.three_c = three_c;
    m.heatmap = make_heatmap(level, max_heatmap_bins);
    return m;
}

HierarchyMetrics hierarchy_metrics(const HierarchyConfig& c, const SimulationResult& r) {
    HierarchyMetrics h;
    h.amat_cycles = r.amat_cycles();
    const std::uint64_t n = r.l1.accesses;
    const double t1 = c.l1.hit_latency_cycles, tm = c.memory_latency_cycles;
    h.latency.l1_cycles = std::uint64_t{c.l1.hit_latency_cycles} * n;
    if (r.l2) {
        h.latency.l2_cycles = std::uint64_t{c.l2->hit_latency_cycles} * r.l2->accesses;
        h.latency.memory_cycles = std::uint64_t{c.memory_latency_cycles} * r.l2->misses;
    } else {
        h.latency.memory_cycles = std::uint64_t{c.memory_latency_cycles} * r.memory.reads;
    }
    if (n != 0) {
        // Textbook form. "Effective" miss ratios exclude write-around misses, which cost t1.
        const double m1 = static_cast<double>(r.l1.misses - r.l1.no_allocate_misses) / static_cast<double>(n);
        if (r.l2) {
            const double t2 = c.l2->hit_latency_cycles;
            const double m2 = r.l2->accesses == 0 ? 0.0
                                                  : static_cast<double>(r.l2->misses) / static_cast<double>(r.l2->accesses);
            h.amat_textbook_cycles = t1 + m1 * (t2 + m2 * tm);
        } else {
            h.amat_textbook_cycles = t1 + m1 * tm;
        }
        if (h.amat_cycles && *h.amat_cycles > 0) h.speedup_vs_memory = tm / *h.amat_cycles;
    }
    const std::uint64_t line = c.l1.line_bytes;
    h.memory_read_bytes = r.memory.reads * line;
    h.memory_write_bytes = r.memory.writes * line;
    if (n != 0) {
        h.memory_bytes_per_access = static_cast<double>(h.memory_read_bytes + h.memory_write_bytes) / static_cast<double>(n);
        h.memory_accesses_per_kilo_access = 1000.0 * static_cast<double>(r.memory.reads + r.memory.writes) / static_cast<double>(n);
    }
    return h;
}

}  // namespace ccsim
