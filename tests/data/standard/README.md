# Standard dataset: `matmul64-O0.trace.gz`

The one shared benchmark workload for everyone working on the simulator. Benchmarks, regression baselines, performance claims and any "config A vs config B" result in a PR use this file unless the PR says otherwise.

## Manifest

| Field | Value |
|---|---|
| File | `tests/data/standard/matmul64-O0.trace.gz` (moved unchanged from `local-traces/matmul.trace.gz`) |
| SHA-256 (gzip file) | `9619648721957a502f486f5290bd924a5e52592f59dad1b34e1d445a4dd3086e` |
| SHA-256 (decompressed text) | `3034a01e770bcbd641e50e881c3d9a01cef9e15196a276fe1fde5a00ff168db0` |
| Record fingerprint (FNV-1a 64, engine) | `0xda2eb04afb87c2d8` |
| Accesses | 4,051,312 (3,492,124 R, 559,188 W; 13.8% writes) |
| Size | 1.2 MB gzip, 58.6 MB text, 32.5 MiB in the engine (8.4 bytes/access) |
| Footprint | 2,934 distinct 64 B lines = 183.4 KiB |
| Format | ccsim trace v1 ([TECHNICAL_DESIGN.md section 7](../../../docs/TECHNICAL_DESIGN.md#7-trace-format-v1)); 2 header comments, 0 invalid lines |
| Source program | [`matmul.c`](matmul.c): 64×64 `double` matrix multiply, `gcc -O0` |
| Recorded with | `valgrind --tool=lackey --trace-mem=yes ./matmul`, converted with awk: instruction fetches and access sizes dropped, `M` (modify) split into `R` then `W` |
| Pinned by | `tests/dataset/test_standard_dataset.cpp` (`dataset.integrity`, `dataset.regression_baseline`) |

Check a copy by hand: `sha256sum tests/data/standard/matmul64-O0.trace.gz`.

## What is in it

| Address region | Share | What it is |
|---|---|---|
| `0x1ffef…` (stack) | 73.4% | Loop counters `i`, `j`, `k` and temporaries, reloaded on every iteration because of `-O0` |
| `0x040…` | 26.3% | The three 32 KiB matrices `a`, `b`, `c` (96 KiB) and the binary's data |
| `0x048…`, `0x04a…`, `0x1fff…` | 0.37% | Dynamic loader, libc start-up and `printf` |

Known limitations, kept on purpose rather than "fixed" by editing real data:

- **Access sizes were dropped**, so the 756 accesses (0.02%) that could straddle a 64 B line boundary touch only their first line. This matches the format v1 rule "one line per access".
- **`-O0` codegen**: a `-O2` build would keep the loop counters in registers and remove most of the stack traffic. This trace therefore models unoptimised code, which suits teaching: it gives heavy temporal locality plus a cache-hostile column walk of `b[k][j]`.
- About **0.4% start-up noise** from the loader and libc. It doesn't move any rate in the reports by a measurable amount.

## Fitness for benchmarking (evidence)

`ctest --test-dir build -L dataset` runs 14 criteria and writes the evidence to [`tests/results/standard_dataset_report.md`](../../results/standard_dataset_report.md). Summary of the current run:

- **Every gate passes.** The trace separates configurations by capacity (FA-LRU miss ratio 1 KiB vs 64 KiB: 83×), by associativity (32 KiB direct-mapped vs 8-way: 0.0221 vs 0.0119 miss rate), by policy (LRU/FIFO/Random: 7.2% spread at 8 KiB), and by inclusion. Results are deterministic.
- **Finding C4, default L2 too big:** the 183 KiB footprint fits inside the 256 KiB default L2, so with the teaching defaults L2 sees only cold misses (2,934, constant across every L1 configuration) and there are no memory write-backs. **Use the `benchmark` profile below for any L2, inclusion or write-back experiment.**
- **Finding C12, hot sets:** the hottest L1 set receives 47× the mean set's accesses (the stack counters). That's realistic for `-O0` code and makes conflict behaviour visible. It isn't a defect, but don't use this trace to judge set-index hashing.
- **Finding C10b, small inclusion effect:** exclusive vs non-inclusive changes memory reads by about 1.2%. That's expected physics: exclusivity adds only L1's capacity, 1/8 of L2 at the benchmark profile.
- **LRU pathology worth knowing:** at 32 KiB, a 16-way L1 misses 4× more than a 2-way one. The column walk cycles through more lines per set than the ways can hold, so LRU always evicts the line needed next. The 3C breakdown labels these as "capacity" misses. This is a real behaviour of LRU, not a simulator bug: three independent FA-LRU implementations agree in `analytics.reuse_profile_survives_compaction`.

## Standard profiles

| Profile | L1 | L2 | Memory | Use for |
|---|---|---|---|---|
| `teaching` | 32 KiB, 64 B, 8-way, LRU, 4 c | 256 KiB, 64 B, 8-way, LRU, 12 c | 200 c | L1 experiments; default UI demo |
| `benchmark` | **8 KiB**, 64 B, 8-way, LRU, 4 c | **64 KiB**, 64 B, 8-way, LRU, 12 c | 200 c | Anything involving L2, inclusion, write policy, memory traffic |

Pinned regression baselines (non-inclusive, write-back, write-allocate):

| Profile | L1 misses | L2 misses | Memory writes | Total latency (cycles) | AMAT |
|---|---|---|---|---|---|
| `teaching` | 48,219 | 2,934 | 0 | 17,370,676 | 4.2877 |
| `benchmark` | 270,833 | 3,536 | 1,444 | 20,162,444 | 4.9768 |

These are **regression baselines**, pinned after the engine passed the hand-derived goldens and the invariant suite. They are not independent goldens. If an engine change moves them, the PR must explain why. Never re-pin them silently.

## Regenerating (needs Valgrind; Linux)

```bash
gcc -O0 -o matmul tests/data/standard/matmul.c
valgrind --tool=lackey --trace-mem=yes --log-file=lackey.log ./matmul
# lackey lines: " L addr,size" load, " S addr,size" store, " M addr,size" modify, "I addr,size" fetch
awk 'BEGIN{print "# source: valgrind --tool=lackey --trace-mem=yes ./matmul (N=64, gcc -O0)";
           print "# converted with awk; instruction fetches and access sizes dropped; M -> R,W"}
     $1=="L"{split($2,a,","); print "R 0x" a[1]}
     $1=="S"{split($2,a,","); print "W 0x" a[1]}
     $1=="M"{split($2,a,","); print "R 0x" a[1]; print "W 0x" a[1]}' lackey.log | gzip -9 > matmul64-O0.trace.gz
```

A regenerated file will **not** be byte-identical. Compiler, libc and Valgrind versions change the addresses. Treat a regenerated file as a new dataset with a new name and a new manifest; don't overwrite this one.
