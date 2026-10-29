// tests/support/reference_policies.hpp
//
// TEST-ONLY stand-ins for the replacement policies. The production policies belong to the
// policy owner (include/ccsim/policy/, src/policy/); these exist so the engine can be
// tested before those land, exactly as CONTRIBUTING section 3 prescribes ("Role 2 uses a
// stub policy until M2-03 merges"). They follow TECHNICAL_DESIGN 5.4 literally and are
// deliberately simple (O(ways) victim scans) so they are easy to trust.
//
// Do not link these into ccsim_core, the CLI or the WASM build.

#pragma once

#include <cstdint>
#include <memory>

#include "ccsim/policy/replacement_policy.hpp"

namespace ccsim::testing {

std::unique_ptr<ReplacementPolicy> make_reference_policy(PolicyKind kind, std::uint32_t sets,
                                                         std::uint32_t ways, std::uint64_t seed);

}  // namespace ccsim::testing
