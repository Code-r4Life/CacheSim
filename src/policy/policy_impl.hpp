// src/policy/policy_impl.hpp
//
// PRIVATE header: only files inside src/policy/ include it (nobody else may, see
// ARCHITECTURE section 12). It lets policy_factory.cpp reach the three policies without
// exposing their classes to the rest of the project. Each function below is defined in
// the .cpp file named after the policy.

#pragma once

#include <cstdint>
#include <memory>

#include "ccsim/policy/replacement_policy.hpp"

namespace ccsim {

std::unique_ptr<ReplacementPolicy> make_lru_policy(std::uint32_t sets, std::uint32_t ways);
std::unique_ptr<ReplacementPolicy> make_fifo_policy(std::uint32_t sets, std::uint32_t ways);
std::unique_ptr<ReplacementPolicy> make_random_policy(std::uint32_t ways, std::uint64_t seed);

}  // namespace ccsim
