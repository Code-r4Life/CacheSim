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

# Portable core. Each module directory src/<module>/ owns a CMakeLists.txt that appends
# its sources to ccsim_core with target_sources(). Modules are discovered automatically, so
# adding a module never edits this file (merge-conflict rule, docs/ARCHITECTURE.md 12).
# src/wasm/ is excluded: it is a separate target layered on top of the core.
add_library(ccsim_core STATIC src/cli.cpp)
target_include_directories(ccsim_core PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_compile_definitions(ccsim_core PRIVATE CCSIM_VERSION="${PROJECT_VERSION}")
ccsim_set_warnings(ccsim_core)

file(GLOB CCSIM_MODULE_LISTS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*/CMakeLists.txt")
list(SORT CCSIM_MODULE_LISTS)
foreach(module_list IN LISTS CCSIM_MODULE_LISTS)
  get_filename_component(module_dir "${module_list}" DIRECTORY)
  get_filename_component(module_name "${module_dir}" NAME)
  if(NOT module_name STREQUAL "wasm")
    add_subdirectory("${module_dir}")
  endif()
endforeach()

# True once the policy owner's src/policy/ defines make_policy(); tests then also run the
# golden suite against the production policies, not only the test stand-ins.
if(EXISTS "${PROJECT_SOURCE_DIR}/src/policy/CMakeLists.txt")
  set(CCSIM_HAS_PRODUCTION_POLICIES ON)
else()
  set(CCSIM_HAS_PRODUCTION_POLICIES OFF)
endif()

# C ABI used by the browser build (src/wasm/). Built natively too so tests can call it.
add_subdirectory(src/wasm)

add_executable(ccsim src/main.cpp)
target_link_libraries(ccsim PRIVATE ccsim_core)
ccsim_set_warnings(ccsim)

if(CCSIM_BUILD_TESTS AND NOT EMSCRIPTEN)
  enable_testing()
  add_subdirectory(tests)
endif()
