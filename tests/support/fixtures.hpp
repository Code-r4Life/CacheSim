// tests/support/fixtures.hpp
//
// Shared test helpers: config builders, trace builders, synthetic workloads, gzip loading
// and the hierarchy invariant checker (TECHNICAL_DESIGN 6 + ARCHITECTURE 5.4).

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ccsim/io/trace.hpp"
#include "ccsim/session/session.hpp"
#include "ccsim/sim/hierarchy.hpp"
#include "reference_policies.hpp"

namespace ccsim::testing {

inline constexpr PolicyFactory kRef = &make_reference_policy;

CacheConfig level(const char* name, std::uint64_t size, std::uint32_t line, std::uint32_t ways,
                  PolicyKind policy, std::uint32_t latency);
HierarchyConfig l1_only(std::uint64_t size, std::uint32_t line, std::uint32_t ways, PolicyKind policy,
                        std::uint32_t l1_latency, std::uint32_t memory_latency);
// The teaching defaults of TECHNICAL_DESIGN section 3.
HierarchyConfig default_hierarchy();

std::shared_ptr<Trace> trace_from_text(const std::string& text, const std::string& name = "inline");
std::shared_ptr<Trace> load_trace_file(const std::string& path);  // .trace or .trace.gz (zlib)
bool gzip_supported() noexcept;

// Deterministic synthetic workloads (splitmix64-seeded, identical on every OS).
std::shared_ptr<Trace> synthetic_random(std::size_t n, std::uint64_t footprint_bytes, double write_ratio,
                                        std::uint64_t seed);
std::shared_ptr<Trace> synthetic_stride(std::size_t n, std::uint64_t stride, std::uint64_t footprint_bytes);
// Mixed phases: sequential sweep, hot set, strided conflicts, random tail.
std::shared_ptr<Trace> synthetic_mixed(std::size_t n, std::uint64_t seed);

// Simulates `trace` access by access and returns the final result.
SimulationResult simulate(const Trace& trace, const HierarchyConfig& config, PolicyFactory factory = kRef);

// Checks every hierarchy invariant; reports failures through the harness with `context`.
void check_invariants(const SimulationResult& r, const HierarchyConfig& config, const std::string& context);

std::string results_dir();  // tests/results (created on demand)
std::string data_dir();     // tests/data
void write_text_file(const std::string& path, const std::string& text);

}  // namespace ccsim::testing
