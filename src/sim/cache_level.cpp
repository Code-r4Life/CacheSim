#include "ccsim/sim/cache_level.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace ccsim {
namespace {

bool is_power_of_two(std::uint64_t v) noexcept { return v != 0 && (v & (v - 1)) == 0; }

std::uint32_t log2_exact(std::uint64_t v) noexcept {
    std::uint32_t bits = 0;
    while ((std::uint64_t{1} << bits) < v) ++bits;
    return bits;
}

}  // namespace

Geometry Geometry::from(const CacheConfig& c) {
    const std::string who = c.name.empty() ? std::string("cache") : c.name;
    if (!is_power_of_two(c.line_bytes)) {
        throw ConfigError(who + ".line: must be a power of two, got " + std::to_string(c.line_bytes));
    }
    if (c.ways == 0) throw ConfigError(who + ".ways: must be at least 1");
    const std::uint64_t set_bytes = std::uint64_t{c.line_bytes} * c.ways;
    if (c.size_bytes == 0 || c.size_bytes % set_bytes != 0) {
        throw ConfigError(who + ".size: " + std::to_string(c.size_bytes) +
                          " is not a positive multiple of line * ways = " + std::to_string(set_bytes));
    }
    const std::uint64_t sets = c.size_bytes / set_bytes;
    if (!is_power_of_two(sets) || sets > 0x80000000ULL) {
        throw ConfigError(who + ".size: derived set count " + std::to_string(sets) +
                          " must be a power of two (and at most 2^31)");
    }
    if (sets * c.ways > 0xFFFFFFFEULL) throw ConfigError(who + ": too many lines");
    if (c.hit_latency_cycles == 0) throw ConfigError(who + ".latency: must be at least 1");

    Geometry g;
    g.line_bytes = c.line_bytes;
    g.ways = c.ways;
    g.sets = static_cast<std::uint32_t>(sets);
    g.offset_bits = log2_exact(c.line_bytes);
    g.index_bits = log2_exact(sets);
    g.set_mask = sets - 1;
    return g;
}

CacheLevel::CacheLevel(const CacheConfig& config, std::unique_ptr<ReplacementPolicy> policy)
    : config_(config), geo_(Geometry::from(config)), policy_(std::move(policy)) {
    if (!policy_) throw ConfigError(config_.name + ".policy: no replacement policy supplied");
    const std::size_t slots = static_cast<std::size_t>(geo_.capacity_lines());
    tags_.assign(slots, 0);
    state_.assign(slots, 0);
    filled_.assign(geo_.sets, 0);
    set_accesses_.assign(geo_.sets, 0);
    set_misses_.assign(geo_.sets, 0);
    if (geo_.ways > kLinearScanMaxWays) index_.emplace(slots);
}

std::uint32_t CacheLevel::choose_way(std::uint32_t set, bool* from_policy) {
    if (filled_[set] < geo_.ways) {
        const std::size_t base = static_cast<std::size_t>(set) * geo_.ways;
        for (std::uint32_t w = 0; w < geo_.ways; ++w) {
            if ((state_[base + w] & kValid) == 0) {
                *from_policy = false;
                return w;
            }
        }
    }
    *from_policy = true;
    const std::uint32_t way = policy_->select_victim(set);
    if (way >= geo_.ways) {
        throw std::logic_error(config_.name + ": replacement policy returned way " +
                               std::to_string(way) + " outside [0, " + std::to_string(geo_.ways) + ")");
    }
    return way;
}

Victim CacheLevel::take(std::uint32_t set, std::uint32_t way) noexcept {
    const std::size_t s = slot(set, way);
    Victim v;
    if ((state_[s] & kValid) == 0) return v;
    v.valid = true;
    v.dirty = (state_[s] & kDirty) != 0;
    v.line = geo_.line_from(tags_[s], set);
    state_[s] = 0;
    --filled_[set];
    if (index_) index_->erase(v.line);
    return v;
}

void CacheLevel::install(std::uint32_t set, std::uint32_t way, std::uint64_t line, bool dirty) {
    const std::size_t s = slot(set, way);
    tags_[s] = geo_.tag_of(line);
    state_[s] = static_cast<std::uint8_t>(kValid | (dirty ? kDirty : 0));
    ++filled_[set];
    if (index_) index_->put(line, static_cast<std::uint32_t>(s));
    policy_->on_fill(set, way);
}

std::size_t CacheLevel::memory_bytes() const noexcept {
    std::size_t bytes = tags_.capacity() * 8 + state_.capacity() + filled_.capacity() * 4 +
                        (set_accesses_.capacity() + set_misses_.capacity()) * 8;
    if (index_) bytes += index_->memory_bytes();
    return bytes;
}

}  // namespace ccsim
