// include/ccsim/report/json.hpp
//
// Dependency-free streaming JSON writer plus the serialisers for run reports
// ("ccsim.simulation/1", extended additively) and single access events ("ccsim.events/1").
//
// Determinism: doubles use std::to_chars shortest round-trip form, so the same result
// prints byte-identically on every platform. 64-bit identifiers that may exceed 2^53
// (addresses, lines, fingerprints) are emitted as "0x..." strings because JavaScript
// numbers cannot hold them exactly; counters stay JSON integers.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ccsim/analytics/metrics.hpp"
#include "ccsim/sim/events.hpp"
#include "ccsim/sim/types.hpp"

namespace ccsim {

class JsonWriter {
public:
    JsonWriter& begin_object();
    JsonWriter& end_object();
    JsonWriter& begin_array();
    JsonWriter& end_array();
    JsonWriter& key(std::string_view k);

    JsonWriter& value(std::string_view s);
    JsonWriter& value(const char* s) { return value(std::string_view(s)); }
    JsonWriter& value(bool b);
    JsonWriter& value(std::uint64_t v);
    JsonWriter& value(std::uint32_t v) { return value(static_cast<std::uint64_t>(v)); }
    JsonWriter& value(double v);
    JsonWriter& value(const std::optional<double>& v);
    JsonWriter& null();
    JsonWriter& hex(std::uint64_t v);

    template <class T>
    JsonWriter& field(std::string_view k, const T& v) {
        key(k);
        return value(v);
    }

    const std::string& str() const noexcept { return out_; }
    std::string take() { return std::move(out_); }

private:
    void separator();
    std::string out_;
    std::vector<bool> first_;  // per open container: no element written yet
    bool after_key_ = false;
};

// Stable lowercase names used in JSON, CSV, logs and the web UI.
std::string_view to_string(PolicyKind p) noexcept;
std::string_view to_string(InclusionPolicy p) noexcept;
std::string_view to_string(WritePolicy p) noexcept;
std::string_view to_string(WriteMissPolicy p) noexcept;
std::string_view to_string(Outcome o) noexcept;
std::string_view to_string(ActionKind k) noexcept;

// One-line reproducible description, e.g.
// "L1 32768B/64B/8w lru wb+wa 4c | L2 262144B/64B/8w lru wb+wa 12c | mem 200c | non_inclusive | seed 1"
std::string describe(const HierarchyConfig& config);

void write_config(JsonWriter& w, const HierarchyConfig& config);
void write_report(JsonWriter& w, const RunReport& report, std::string_view ccsim_version);
std::string report_to_json(const RunReport& report, std::string_view ccsim_version);

// One NDJSON line (no trailing newline) of schema "ccsim.events/1".
std::string event_to_json(const AccessEvent& event);

}  // namespace ccsim
