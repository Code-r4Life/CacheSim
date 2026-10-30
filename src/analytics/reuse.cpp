#include "ccsim/analytics/reuse.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "ccsim/sim/line_index.hpp"

namespace ccsim {
namespace {

std::uint32_t bucket_of(std::uint64_t distance) noexcept {
    std::uint32_t b = 0;
    while (distance != 0) {
        ++b;
        distance >>= 1;
    }
    return b;  // 0 -> 0, 1 -> 1, 2..3 -> 2, 4..7 -> 3, ...
}

// Fenwick tree over timestamps 0..n-1 holding 0/1 marks.
class Fenwick {
public:
    void reset(std::size_t n) { tree_.assign(n + 1, 0); }
    std::size_t size() const noexcept { return tree_.size() - 1; }
    void add(std::size_t pos, std::int32_t delta) noexcept {
        for (std::size_t i = pos + 1; i < tree_.size(); i += i & (~i + 1)) {
            tree_[i] = static_cast<std::uint32_t>(static_cast<std::int64_t>(tree_[i]) + delta);
        }
    }
    std::uint64_t prefix(std::size_t pos) const noexcept {  // marks in [0, pos]
        std::uint64_t s = 0;
        for (std::size_t i = pos + 1; i > 0; i -= i & (~i + 1)) s += tree_[i];
        return s;
    }

private:
    std::vector<std::uint32_t> tree_;
};

}  // namespace

ReuseProfile reuse_profile(const Trace& trace, std::uint32_t line_bytes) {
    if (line_bytes == 0 || (line_bytes & (line_bytes - 1)) != 0) {
        throw std::invalid_argument("reuse_profile: line_bytes must be a power of two");
    }
    std::uint32_t shift = 0;
    while ((1U << shift) < line_bytes) ++shift;

    ReuseProfile p;
    p.line_bytes = line_bytes;
    p.accesses = trace.size();
    p.histogram.assign(1, 0);

    constexpr std::size_t kMinCapacity = 1U << 16;
    LineIndex last;  // line -> timestamp of latest use
    Fenwick marks;
    marks.reset(kMinCapacity);
    std::size_t now = 0;

    auto compact = [&]() {
        std::vector<std::pair<std::uint32_t, std::uint64_t>> live;  // (timestamp, line)
        live.reserve(last.size());
        last.for_each([&](std::uint64_t line, std::uint32_t ts) { live.emplace_back(ts, line); });
        std::sort(live.begin(), live.end());
        marks.reset(std::max(kMinCapacity, live.size() * 2));
        for (std::size_t i = 0; i < live.size(); ++i) {
            last.put(live[i].second, static_cast<std::uint32_t>(i));
            marks.add(i, 1);
        }
        now = live.size();
    };

    for (std::size_t i = 0; i < trace.size(); ++i) {
        if (now == marks.size()) compact();
        const std::uint64_t line = trace.address(i) >> shift;
        const std::uint32_t prev = last.find(line);
        if (prev == LineIndex::kAbsent) {
            ++p.cold;
        } else {
            // Every live mark is < now, so marks after `prev` = live lines - prefix(prev).
            const std::uint64_t distance = last.size() - marks.prefix(prev);
            const std::uint32_t b = bucket_of(distance);
            if (b >= p.histogram.size()) p.histogram.resize(b + 1, 0);
            ++p.histogram[b];
            marks.add(prev, -1);
        }
        marks.add(now, 1);
        last.put(line, static_cast<std::uint32_t>(now));
        ++now;
    }
    return p;
}

std::uint64_t ReuseProfile::fa_lru_misses_pow2(std::uint32_t k) const noexcept {
    // Capacity C = 2^k lines misses every distance >= C, i.e. buckets b >= k + 1.
    std::uint64_t misses = cold;
    for (std::size_t b = static_cast<std::size_t>(k) + 1; b < histogram.size(); ++b) misses += histogram[b];
    return misses;
}

double ReuseProfile::fa_lru_miss_ratio_pow2(std::uint32_t k) const noexcept {
    return accesses == 0 ? 0.0
                         : static_cast<double>(fa_lru_misses_pow2(k)) / static_cast<double>(accesses);
}

std::uint64_t ReuseProfile::median_distance_upper_bound() const noexcept {
    std::uint64_t total = 0;
    for (const auto c : histogram) total += c;
    if (total == 0) return 0;
    std::uint64_t seen = 0;
    for (std::size_t b = 0; b < histogram.size(); ++b) {
        seen += histogram[b];
        if (seen * 2 >= total) return b == 0 ? 0 : (std::uint64_t{1} << b) - 1;
    }
    return 0;
}

}  // namespace ccsim
