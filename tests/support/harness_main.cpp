#include <chrono>
#include <cstring>
#include <exception>
#include <iostream>

#include "harness.hpp"

namespace ccsim::testing {

namespace {
int g_failures = 0;
const char* g_current = "";
}  // namespace

std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

void report_failure(const char* file, int line, const std::string& message) {
    ++g_failures;
    std::cerr << "FAILED [" << g_current << "] " << file << ':' << line << ": " << message << '\n';
}

}  // namespace ccsim::testing

int main(int argc, char** argv) {
    using namespace ccsim::testing;
    const char* filter = argc > 1 ? argv[1] : "";
    int run = 0, skipped = 0;
    for (const TestCase& t : registry()) {
        if (std::strstr(t.name, filter) == nullptr) continue;
        g_current = t.name;
        const int before = g_failures;
        const auto start = std::chrono::steady_clock::now();
        try {
            t.body();
        } catch (const SkipTest& s) {
            ++skipped;
            std::cout << "SKIP  " << t.name << ": " << s.reason << '\n';
            continue;
        } catch (const std::exception& e) {
            report_failure("<exception>", 0, e.what());
        }
        const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        ++run;
        std::cout << (g_failures == before ? "ok    " : "FAIL  ") << t.name << "  (" << ms << " ms)\n";
    }
    std::cout << run << " test(s) run, " << skipped << " skipped, " << g_failures << " check(s) failed\n";
    if (g_failures != 0) return 1;
    if (run == 0 && skipped > 0) return 77;  // everything skipped: CTest reports "skipped"
    return 0;
}
