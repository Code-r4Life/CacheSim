# Development Guide

Each command block is labelled **Verified** (executed, output matched) or **Not tested**. So far only the Linux paths have been executed; the README [platform matrix](../README.md#platform-validation-matrix) is the summary.

## 1. Toolchains

| OS | Compiler | Build tools | Tested with | Expected minimum (not tested) |
|---|---|---|---|---|
| Linux | GCC or Clang | CMake 3.20+, Make or Ninja | GCC 13.3, Clang 18.1, CMake 3.20.5 and 3.28.3 | GCC 9, Clang 9 |
| Windows | MSVC (Visual Studio 2022) | CMake 3.20+, PowerShell | none | Visual Studio 2022 |
| macOS | Apple Clang (Xcode Command Line Tools) | CMake 3.20+ | none | macOS 10.15 SDK (for `std::filesystem`) |

CMake 3.20 is a hard minimum (`cmake_minimum_required`); it is also the first version whose `ctest` supports `--test-dir`.

## 2. Prerequisites

### Linux

```bash
# Debian / Ubuntu
sudo apt update && sudo apt install -y build-essential cmake git
# Fedora
sudo dnf install -y gcc-c++ cmake make git
# Arch
sudo pacman -S --needed base-devel cmake git
# Optional: Clang and Ninja (Debian / Ubuntu)
sudo apt install -y clang ninja-build
```

Check `cmake --version`. If your distribution ships CMake older than 3.20, install a current one with `python3 -m pip install --user cmake` and make sure `~/.local/bin` comes first on `PATH`.

### Windows

Run in PowerShell. Not tested.

```powershell
# C++ build tools (or install Visual Studio 2022 Community with "Desktop development with C++")
winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override "--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
# CMake and Git
winget install --id Kitware.CMake -e
winget install --id Git.Git -e
```

Open a new PowerShell window so `PATH` changes apply, then check `cmake --version`. Visual Studio also ships CMake inside "Developer PowerShell for VS 2022".

### macOS

Not tested.

```bash
xcode-select --install   # Apple Clang, SDK, make
brew install cmake       # or install CMake.app from cmake.org and add its CLI to PATH
```

Full Xcode is needed only for the Instruments workflow in section 7.

## 3. Get the source

```bash
git clone <repository-url> cpu-cache-simulator
cd cpu-cache-simulator
git switch dev            # integration branch; see CONTRIBUTING.md section 6
```

## 4. Build and test

Always build out of source; `build*/` directories are git-ignored.

### Linux (GCC): Verified

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/ccsim --version
```

Expected test result:

```text
    Start 1: smoke.cli_contract
1/2 Test #1: smoke.cli_contract ...............   Passed    0.01 sec
    Start 2: smoke.executable_version
2/2 Test #2: smoke.executable_version .........   Passed    0.00 sec

100% tests passed, 0 tests failed out of 2
```

### Linux (Clang): Verified

```bash
cmake -S . -B build-clang -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++
cmake --build build-clang
ctest --test-dir build-clang --output-on-failure
```

### macOS (Apple Clang): Not tested

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/ccsim --version
```

### Windows (MSVC, PowerShell): Not tested

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\ccsim.exe --version
```

Visual Studio generators are multi-configuration: pick the configuration when building and testing (`--config`, `-C`), not with `CMAKE_BUILD_TYPE`. If several Visual Studio versions are installed, select one with `-G "Visual Studio 17 2022"`. The same multi-configuration flow is Verified on Linux with `-G "Ninja Multi-Config"`, and the sources cross-compile for Windows with MinGW-w64; MSVC itself has not been run.

### Build options

| Option | Default | Effect |
|---|---|---|
| `CMAKE_BUILD_TYPE` | `Release` (single-configuration generators only) | `Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel` |
| `CCSIM_BUILD_TESTS` | `ON` | Build tests and register them with CTest |
| `CCSIM_WARNINGS_AS_ERRORS` | `OFF` | Adds `-Werror` or `/WX`; CI turns it on |

Add `--parallel <jobs>` to `cmake --build` for faster builds and `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` (Make and Ninja generators) for clangd.

## 5. Running tests

```bash
ctest --test-dir build --output-on-failure      # everything
ctest --test-dir build -L smoke                 # by label
ctest --test-dir build -R cli_contract -V       # by name pattern, verbose
ctest --test-dir build -LE hardware             # skip tests that need real counters (label planned)
```

On Windows and other multi-configuration generators, add `-C Release` (or the configuration you built). Current label: `smoke`. Planned labels: `unit`, `e2e`, `hardware`, `bench` ([CONTRIBUTING.md section 8](../CONTRIBUTING.md#8-testing-standards)).

## 6. Debugging

Debug build (Verified on Linux, including the `gdb` line; macOS not tested):

```bash
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
gdb --args ./build-debug/ccsim --help    # Linux
lldb -- ./build-debug/ccsim --help       # macOS (not tested)
```

Windows (not tested): run `cmake --build build --config Debug`, open `build\cpu_cache_simulator.sln` in Visual Studio, set `ccsim` as the startup project, and enter arguments under Properties > Debugging. Alternatively open the source folder directly; Visual Studio reads `CMakeLists.txt`.

Sanitizers (Verified on Linux; macOS not tested):

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

An MSVC AddressSanitizer option is planned (issue M2-15).

## 7. Monitoring prerequisites

`ccsim monitor` is not implemented yet. These checks show what a machine exposes so Roles 4 and 7 can plan and record evidence. None has been run on real hardware: the validation VM has no PMU.

### Linux

```bash
# Install perf
sudo apt install -y linux-tools-common linux-tools-$(uname -r)   # Ubuntu
sudo apt install -y linux-perf                                   # Debian
sudo dnf install -y perf                                         # Fedora

cat /proc/sys/kernel/perf_event_paranoid
perf list hw cache
perf stat -e cycles,instructions,L1-dcache-loads,L1-dcache-load-misses,LLC-loads,LLC-load-misses -- ls > /dev/null
```

In `perf stat` output, `<not supported>` means the CPU or kernel does not expose the event (typical in VMs); `<not counted>` means the event never got a hardware counter. If `perf_event_paranoid` is above 2, either lower it until the next reboot on a machine you control (`sudo sysctl -w kernel.perf_event_paranoid=2`) or grant `CAP_PERFMON` (Linux 5.8+). Do not make permanent changes on shared machines.

### Windows

Run in an elevated PowerShell (recording PMU data requires Administrator rights):

```powershell
wpr -pmcsources
```

The output lists the PMU profile sources this CPU and Windows build expose; names are CPU-specific. Results inside VMs or with Hyper-V or virtualization-based security enabled are Not tested.

### macOS

```bash
sudo powermetrics --samplers cpu_power -i 1000 -n 1   # power and frequency only, no cache counters
xcrun xctrace list templates                          # needs full Xcode; look for "CPU Counters"
```

Recording cache events requires a preconfigured template; see [WORKING_DEMO.md section 3](WORKING_DEMO.md#3-monitoring-workflow-planned).

## 8. Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `CMake 3.20 or higher is required.  You are running version 3.16.8` | Distribution CMake too old | Install a newer CMake (section 2) |
| `ctest --test-dir build` prints `No tests were found!!!` | `ctest` older than 3.20 silently ignores `--test-dir` (often an old CTest earlier on `PATH`) | Check `ctest --version`, fix `PATH`, or run `cd build && ctest` |
| `Test not available without configuration.  (Missing "-C <config>"?)` | Multi-configuration generator (Visual Studio, Xcode, Ninja Multi-Config) | Add `-C Release`, or whichever configuration you built |
| `Error: generator : Ninja` / `Does not match the generator used previously` | Reusing a build directory with a different generator | Delete that build directory or use a new `-B` path |
| `No CMAKE_CXX_COMPILER could be found` | No C++ compiler installed or visible | Install per section 2; on Windows use Developer PowerShell or install the C++ workload |
| `cmake` is not recognized (PowerShell) | `PATH` not refreshed after install | Open a new terminal or use Developer PowerShell for VS 2022 |
| `xcrun: error: invalid active developer path` (macOS) | Command Line Tools missing, often after an OS upgrade | `xcode-select --install` |
| `perf` missing or reports no tools for the running kernel | `perf` must match the kernel | Install the kernel-specific package (section 7) |
| `perf` permission errors | `perf_event_paranoid` too strict | Section 7: lower it temporarily or grant `CAP_PERFMON` |
| Hardware events show `<not supported>` | VM or container without a virtualized PMU, or event absent on this CPU | Use bare metal or a VM with PMU passthrough; `ccsim` reports such metrics as `unsupported` |
| A test fails in Debug but passes in Release | The check uses `assert()`, which `NDEBUG` removes in Release | Use the test framework's checks (CONTRIBUTING.md section 8) |
