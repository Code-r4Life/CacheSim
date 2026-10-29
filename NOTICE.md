# NOTICE: configuration track, read before your next change

**To:** the configuration and policies track (GitHub `Code-r4Life`)
**From:** the simulation track
**Date:** 2026-10-29
**Branch with these changes:** `feat/sim-engine`

The simulation engine has landed on the branch above, built against the contracts you wrote. It is fully tested, using test-only stand-ins in place of your policies. This page lists everything you need to know and follow so our work keeps merging cleanly. Full details: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), especially sections 5, 11 and 12, and [AGENTS.md](AGENTS.md).

## 1. What changed that affects you

| Change | Where | What you do |
|---|---|---|
| **Contract extension 1.1** (append-only): enums `InclusionPolicy`, `WritePolicy`, `WriteMissPolicy`; new fields at the end of `CacheConfig` (`write_policy`, `write_miss_policy`), `HierarchyConfig` (`inclusion`) and `LevelStats` (5 counters) | `include/ccsim/sim/types.hpp` | Pull it before editing that file. Nothing was renamed or reordered, and the defaults keep v1 behaviour, so your existing code still compiles. Your `validate()` must cover the new fields ([ARCHITECTURE 5.1](docs/ARCHITECTURE.md#51-configuration-surface)). |
| Policies are consumed through `PolicyFactory` (same signature as your `make_policy`) | `include/ccsim/sim/hierarchy.hpp` | Implement `make_policy` exactly as declared in your header. No other glue is needed. |
| The build is modular: every `src/<module>/CMakeLists.txt` is discovered automatically | root `CMakeLists.txt` | Create `src/policy/CMakeLists.txt` with `target_sources(ccsim_core PRIVATE lru.cpp …)`. **Don't edit the root `CMakeLists.txt`.** |
| As soon as `src/policy/CMakeLists.txt` exists, the golden suite also runs against **your** policies | `tests/golden/` | Your LRU / FIFO / Random must reproduce Demos A to C exactly (`ctest -L golden`). That is your acceptance test. |
| Test harness | `tests/support/harness.hpp` | Put your tests in `tests/unit/test_policy_<name>.cpp` and `tests/unit/test_config_<name>.cpp`. They are picked up automatically. |
| Standard dataset moved | `local-traces/` → `tests/data/standard/matmul64-O0.trace.gz` | Same bytes (SHA-256 pinned). Use the `benchmark` profile for L2 experiments ([README](tests/data/standard/README.md)). |
| Smoke test moved | `tests/smoke_test.cpp` → `tests/smoke/smoke_test.cpp` | Nothing to do |
| Policy name table added | `kPolicyKindCount`, `kPolicyKindNames` in `types.hpp` | Keep it in sync when you append a `PolicyKind` |
| `.gitattributes` added (LF everywhere) | repo root | If you're on Windows, run `git add --renormalize .` once after pulling |

## 2. Your open deliverables, in order

1. **`make_policy()` with LRU, FIFO, Random** in `src/policy/`, following [TECHNICAL_DESIGN 5.4](docs/TECHNICAL_DESIGN.md#54-replacement-policies). Random = `std::mt19937_64(seed)`, victim = `rng() % ways`, advanced only when a full set needs a victim. **Never** `std::uniform_int_distribution`.
2. **`validate(const HierarchyConfig&)`** in `src/config/validate.cpp` (new module folder; add its own `CMakeLists.txt`). It throws `ConfigError` naming the field, and covers TECHNICAL_DESIGN section 3 plus the 1.1 fields.
3. **Config presets and spec parsing** (`size=32KiB,line=64,…`) in `include/ccsim/config/` for the CLI and UI.
4. Later: tree-PLRU, NRU, MRU, LFU, Clock, SRRIP/BRRIP. Each one appends a `PolicyKind` value **and** its name to `kPolicyKindNames` (bumping `kPolicyKindCount`) in `types.hpp`. That table is the only place policies are enumerated, so you never need to touch simulation-track files. Recipe: [AGENTS.md section 6](AGENTS.md#6-adding-things-recipes). OPT needs a contract discussion first ([ARCHITECTURE 5.6](docs/ARCHITECTURE.md#56-replacement-policies)).

## 3. Rules that keep us conflict-free

- **Stay inside your paths:** `include/ccsim/policy/`, `src/policy/`, `include/ccsim/config/`, `src/config/`, `tests/unit/test_policy_*`, `tests/unit/test_config_*`. The simulation track never edits these.
- **`include/ccsim/sim/types.hpp` is shared and append-only.** Add at the end, keep the defaults behaviour-preserving, one contract change per PR, and open a `contract` issue first so the other side knows.
- **Don't touch** `tests/support/reference_policies.*`. They stay as the engine's independent, test-only stand-ins. Once your policies pass the goldens, the product (CLI and WASM) links yours, and the stand-ins never ship.
- **Never modify** `tests/data/standard/` or the regression baselines in `tests/dataset/`.
- Keep the engine contracts: `select_victim` is called only on full sets and must return a value in `[0, ways)` (the engine checks this and throws otherwise). `on_hit` is also called for write-backs and forwarded stores that hit. Lines invalidated by the engine (inclusive back-invalidation, exclusive move-out) are refilled through `on_fill`, and no separate invalidate call exists.
- Branch from the latest `main`, keep PRs small, rebase before review, and merge at least weekly.

## 4. Before you open a PR

```bash
git fetch origin && git rebase origin/main
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCCSIM_WARNINGS_AS_ERRORS=ON
cmake --build build && ctest --test-dir build --output-on-failure     # must be 100% green
```

Paste the `ctest` summary into the PR, and state your OS and compiler.

## 5. Questions

Open an issue and tag the simulation track. Contract questions use the `contract` label.
