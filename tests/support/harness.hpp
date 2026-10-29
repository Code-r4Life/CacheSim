// tests/support/harness.hpp
//
// Minimal self-registering test harness (no third-party dependency; doctest can replace it
// later under M1-04 without changing test bodies much). CHECK never uses assert(), so
// checks run in Release builds too (CONTRIBUTING section 8).
//
//   TEST_CASE("sim.lru_demo_c") { CHECK_EQ(r.l1.hits, 2u); }
//
// Each test executable links tests/support/harness_main.cpp; pass a substring to run a
// subset: ./ccsim_unit_tests sim.

#pragma once

#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace ccsim::testing {

struct TestCase {
    const char* name;
    std::function<void()> body;
};

std::vector<TestCase>& registry();
void report_failure(const char* file, int line, const std::string& message);

struct Registrar {
    Registrar(const char* name, std::function<void()> body) { registry().push_back({name, std::move(body)}); }
};

// Exit code that CTest maps to "skipped" (SKIP_RETURN_CODE 77).
struct SkipTest {
    std::string reason;
};

template <class A, class B>
void check_eq(const A& a, const B& b, const char* ea, const char* eb, const char* file, int line) {
    if (!(a == b)) {
        std::ostringstream os;
        os << ea << " == " << eb << "  (" << a << " vs " << b << ")";
        report_failure(file, line, os.str());
    }
}

inline void check_near(double a, double b, double rel, const char* ea, const char* eb, const char* file, int line) {
    const double scale = std::fmax(std::fabs(a), std::fabs(b));
    if (!(std::fabs(a - b) <= rel * (scale > 0 ? scale : 1.0))) {
        std::ostringstream os;
        os.precision(17);
        os << ea << " ~= " << eb << "  (" << a << " vs " << b << ", rel tol " << rel << ")";
        report_failure(file, line, os.str());
    }
}

}  // namespace ccsim::testing

#define CCSIM_CAT2(a, b) a##b
#define CCSIM_CAT(a, b) CCSIM_CAT2(a, b)
#define TEST_CASE(name)                                                                 \
    static void CCSIM_CAT(ccsim_test_, __LINE__)();                                     \
    static const ::ccsim::testing::Registrar CCSIM_CAT(ccsim_reg_, __LINE__)(           \
        name, &CCSIM_CAT(ccsim_test_, __LINE__));                                       \
    static void CCSIM_CAT(ccsim_test_, __LINE__)()

#define CHECK(expr)                                                                     \
    do {                                                                                \
        if (!(expr)) ::ccsim::testing::report_failure(__FILE__, __LINE__, #expr);       \
    } while (0)
#define CHECK_EQ(a, b) ::ccsim::testing::check_eq((a), (b), #a, #b, __FILE__, __LINE__)
#define CHECK_NEAR(a, b, rel) ::ccsim::testing::check_near((a), (b), (rel), #a, #b, __FILE__, __LINE__)
#define CHECK_THROWS_AS(expr, type)                                                     \
    do {                                                                                \
        bool ccsim_threw = false;                                                       \
        try {                                                                           \
            (void)(expr);                                                               \
        } catch (const type&) {                                                         \
            ccsim_threw = true;                                                         \
        } catch (...) {                                                                 \
        }                                                                               \
        if (!ccsim_threw) ::ccsim::testing::report_failure(__FILE__, __LINE__, #expr " throws " #type); \
    } while (0)
