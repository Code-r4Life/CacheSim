// Entry point of the browser module: installs the production policies when they exist.
#include "ccsim/wasm/api_internal.hpp"

int main() {
#ifdef CCSIM_HAS_PRODUCTION_POLICIES
    ccsim::api::set_policy_factory(&ccsim::make_policy);
#endif
    return 0;
}
