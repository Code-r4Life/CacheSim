#include "ccsim/cli.hpp"

#include <ostream>

#ifndef CCSIM_VERSION
#error "CCSIM_VERSION must be defined by the build system (see CMakeLists.txt)"
#endif

namespace ccsim {
namespace {

constexpr std::string_view kUsage =
    "Usage: ccsim <command> [options]\n"
    "\n"
    "Commands:\n"
    "  simulate     Run a memory trace through configurable L1/L2 caches  [Planned]\n"
    "  monitor      Sample CPU performance counters at fixed intervals    [Planned]\n"
    "\n"
    "Options:\n"
    "  -h, --help   Show this help and exit\n"
    "  --version    Show the version and exit\n"
    "\n"
    "Planned commands currently exit with code 3 (not implemented).\n"
    "Design: docs/TECHNICAL_DESIGN.md\n";

}  // namespace

std::string_view version() noexcept {
    return CCSIM_VERSION;
}

ExitCode run(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) {
    if (args.empty()) {
        err << kUsage;
        return ExitCode::UsageError;
    }

    const std::string& command = args.front();

    if (command == "-h" || command == "--help") {
        out << kUsage;
        return ExitCode::Success;
    }
    if (command == "--version") {
        out << "ccsim " << version() << '\n';
        return ExitCode::Success;
    }
    if (command == "simulate" || command == "monitor") {
        err << "ccsim " << command
            << ": not implemented yet (status: Planned). See docs/TECHNICAL_DESIGN.md.\n";
        return ExitCode::NotImplemented;
    }

    err << "ccsim: unknown command or option '" << command << "'\n\n" << kUsage;
    return ExitCode::UsageError;
}

}  // namespace ccsim
