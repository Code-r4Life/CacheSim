// Smoke test: the core library links and the CLI contract holds.
// Deliberately framework-free so the foundation builds with no dependencies.
// Uses its own CHECK instead of assert(), which Release builds compile out.

#include "ccsim/cli.hpp"

#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        ++g_failures;
        std::cerr << "FAILED (smoke_test.cpp:" << line << "): " << expression << '\n';
    }
}

#define CHECK(expr) check(static_cast<bool>(expr), #expr, __LINE__)

struct CliResult {
    ccsim::ExitCode code;
    std::string out;
    std::string err;
};

CliResult run_cli(const std::vector<std::string>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const ccsim::ExitCode code = ccsim::run(args, out, err);
    return {code, out.str(), err.str()};
}

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

}  // namespace

int main() {
    // Version is wired from CMake's project(VERSION ...).
    CHECK(ccsim::version() == CCSIM_EXPECTED_VERSION);

    {
        const CliResult r = run_cli({"--version"});
        CHECK(r.code == ccsim::ExitCode::Success);
        CHECK(r.out == std::string("ccsim ") + CCSIM_EXPECTED_VERSION + "\n");
        CHECK(r.err.empty());
    }
    for (const char* flag : {"--help", "-h"}) {
        const CliResult r = run_cli({flag});
        CHECK(r.code == ccsim::ExitCode::Success);
        CHECK(contains(r.out, "Usage: ccsim"));
        CHECK(contains(r.out, "simulate"));
        CHECK(contains(r.out, "monitor"));
        CHECK(r.err.empty());
    }
    {
        const CliResult r = run_cli({});
        CHECK(r.code == ccsim::ExitCode::UsageError);
        CHECK(r.out.empty());
        CHECK(contains(r.err, "Usage: ccsim"));
    }
    {
        const CliResult r = run_cli({"--bogus"});
        CHECK(r.code == ccsim::ExitCode::UsageError);
        CHECK(r.out.empty());
        CHECK(contains(r.err, "'--bogus'"));
    }
    // Planned modes must say so instead of pretending to work.
    for (const char* command : {"simulate", "monitor"}) {
        const CliResult r = run_cli({command});
        CHECK(r.code == ccsim::ExitCode::NotImplemented);
        CHECK(r.out.empty());
        CHECK(contains(r.err, "not implemented"));
    }
    // Exit-code values are part of the public contract.
    CHECK(static_cast<int>(ccsim::ExitCode::Success) == 0);
    CHECK(static_cast<int>(ccsim::ExitCode::RuntimeError) == 1);
    CHECK(static_cast<int>(ccsim::ExitCode::UsageError) == 2);
    CHECK(static_cast<int>(ccsim::ExitCode::NotImplemented) == 3);

    if (g_failures != 0) {
        std::cerr << g_failures << " smoke check(s) failed\n";
        return 1;
    }
    std::cout << "smoke test passed\n";
    return 0;
}
