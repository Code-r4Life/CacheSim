// include/ccsim/wasm/api_internal.hpp
//
// C++-only hooks of the C ABI layer. The browser build installs the production
// make_policy; native tests install the reference stand-ins from tests/support/.

#pragma once

#include <cstdint>

#include "ccsim/sim/hierarchy.hpp"

namespace ccsim::api {

void set_policy_factory(PolicyFactory factory) noexcept;

// Decodes the uint64 config array documented in ccsim_api.h. Throws ConfigError.
HierarchyConfig decode_config(const std::uint64_t* words, std::uint32_t count);

}  // namespace ccsim::api
