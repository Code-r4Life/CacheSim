cmake_minimum_required(VERSION 3.20)

project(cpu_cache_simulator
  VERSION 0.1.0
  DESCRIPTION "Configurable L1/L2 cache simulator and CPU performance-counter monitor"
  LANGUAGES CXX)

option(CCSIM_BUILD_TESTS "Build the test suite" ON)
option(CCSIM_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Single-config generators (Makefiles, Ninja) build without optimization when no
# build type is given; default to Release so benchmarks are never silently -O0.
get_property(CCSIM_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT CCSIM_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type: Debug, Release, RelWithDebInfo, MinSizeRel" FORCE)
endif()

function(ccsim_set_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
    if(CCSIM_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
    if(CCSIM_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

# Portable core. Platform-specific monitoring backends will be added as separate
# sources selected by CMAKE_SYSTEM_NAME (see docs/TECHNICAL_DESIGN.md).
add_library(ccsim_core STATIC src/cli.cpp)
target_include_directories(ccsim_core PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_compile_definitions(ccsim_core PRIVATE CCSIM_VERSION="${PROJECT_VERSION}")
ccsim_set_warnings(ccsim_core)

add_executable(ccsim src/main.cpp)
target_link_libraries(ccsim PRIVATE ccsim_core)
ccsim_set_warnings(ccsim)

if(CCSIM_BUILD_TESTS)
  enable_testing()

  add_executable(ccsim_smoke_test tests/smoke_test.cpp)
  target_link_libraries(ccsim_smoke_test PRIVATE ccsim_core)
  target_compile_definitions(ccsim_smoke_test PRIVATE CCSIM_EXPECTED_VERSION="${PROJECT_VERSION}")
  ccsim_set_warnings(ccsim_smoke_test)

  # In-process checks of the CLI contract (exit codes, output streams).
  add_test(NAME smoke.cli_contract COMMAND ccsim_smoke_test)
  # The real executable starts and reports the configured version.
  add_test(NAME smoke.executable_version COMMAND ccsim --version)
  set_tests_properties(smoke.executable_version PROPERTIES
    PASS_REGULAR_EXPRESSION "^ccsim ${PROJECT_VERSION}")
  set_tests_properties(smoke.cli_contract smoke.executable_version PROPERTIES LABELS smoke)
endif()
