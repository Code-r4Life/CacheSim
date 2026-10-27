#pragma once

// Command-line entry contract shared by the ccsim executable and its tests.
// Status: Partially implemented. --help and --version work; the simulate and
// monitor commands are recognised but report "not implemented" (exit code 3).

#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace ccsim {

/// Process exit codes. Values are part of the CLI contract; do not renumber.
enum class ExitCode : int {
    Success = 0,
    RuntimeError = 1,   ///< I/O, parse, or platform failure (reserved; not emitted yet).
    UsageError = 2,     ///< Missing or unknown command/option.
    NotImplemented = 3  ///< Recognised command whose feature is still Planned.
};

/// Semantic version of this build (set from CMake's project VERSION).
std::string_view version() noexcept;

/// Runs the CLI. `args` excludes the program name. Normal output goes to `out`,
/// diagnostics to `err`. Never calls std::exit, so it is safe to test in-process.
ExitCode run(const std::vector<std::string>& args, std::ostream& out, std::ostream& err);

}  // namespace ccsim
