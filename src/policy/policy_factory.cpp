// src/policy/policy_factory.cpp
//
// make_policy(): the one function the rest of the project calls to get a policy. The engine
// holds it as a PolicyFactory function pointer (include/ccsim/sim/hierarchy.hpp), which is
// why this must be a plain function and not a class.
//
// To add a policy later: add its value to PolicyKind and its name to kPolicyKindNames in
// types.hpp (one contract PR), write src/policy/<name>.cpp, add one `case` below, and add
// the file to CMakeLists.txt.

#include <cstdint>
#include <memory>

#include "ccsim/policy/replacement_policy.hpp"
#include "ccsim/sim/types.hpp"
#include "policy_impl.hpp"

namespace ccsim {

std::unique_ptr<ReplacementPolicy> make_policy(PolicyKind kind, std::uint32_t sets,
                                               std::uint32_t ways, std::uint64_t seed) {
    // A cache needs at least one set of one way; zero would make every later index invalid.
    if (sets == 0 || ways == 0) {
        throw ConfigError("make_policy: sets and ways must both be at least 1");
    }
    switch (kind) {
        case PolicyKind::Lru:
            return make_lru_policy(sets, ways);
        case PolicyKind::Fifo:
            return make_fifo_policy(sets, ways);
        case PolicyKind::Random:
            return make_random_policy(ways, seed);
    }
    // Only reachable if someone casts a bad number to PolicyKind.
    throw ConfigError("make_policy: unknown PolicyKind");
}

}  // namespace ccsim
