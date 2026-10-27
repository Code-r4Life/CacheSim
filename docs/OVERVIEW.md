# ccsim at a glance

A tool that shows how CPU caches work: **simulate** them from a trace, or **monitor** a real CPU while a program runs.

![Two modes, one core: Simulate turns a memory trace into exact hit, miss and AMAT numbers; Monitor samples real CPU counters and marks missing ones as n/a](assets/overview-modes.svg)

## Why caches matter

![Latency ladder: an L1 hit costs 4 cycles, an L2 hit 16, and main memory 216, which is 54 times an L1 hit](assets/latency-ladder.svg)

Memory is huge but slow. Caches are small and fast. Every miss pushes the request one level further down.

## What happens on every access

```mermaid
flowchart LR
    A["CPU reads or writes<br/>an address"] --> B{"In L1?"}
    B -- "yes" --> H["Hit: done"]
    B -- "no" --> V["Make room in L1<br/>(dirty line goes to L2)"]
    V --> C{"In L2?"}
    C -- "yes" --> F["Copy into L1"]
    C -- "no" --> M["Fetch from memory<br/>into L2, then L1"]
```

Every step is counted: hits, misses, evictions, write-backs.

## How the cache finds a line

![The default L1 splits an address into a 52-bit tag, 6-bit set index and 6-bit byte offset; 0x7ffd1b64 lands in set 45 and hits on a matching tag](assets/address-split.svg)

## Who gets evicted when a set is full

| Policy | Evicts |
|---|---|
| LRU | The line unused for the longest time |
| FIFO | The line that arrived first |
| Random | Any line, from a seeded generator, so runs are repeatable |

## What each OS lets us measure

| | Linux | Windows | macOS |
|---|---|---|---|
| Cycles, instructions | Yes | OS cycle time; more with Administrator | Yes, if the OS reports them |
| L1 data cache | Yes, if the CPU exposes it | Needs Administrator | Instruments only |
| L2 cache | CPU-specific codes | Needs Administrator | Instruments only |

Anything unavailable shows as `n/a` with the reason. This table is the v1 plan; none of it is tested on hardware yet.

## Where we are

```mermaid
flowchart LR
    M1["M1<br/>Foundations<br/>and contracts"] --> M2["M2<br/>Build every<br/>module"] --> M3["M3<br/>Connect<br/>end to end"] --> M4["M4<br/>Verify and<br/>release"]
    NOW(["Today: build, CLI shell,<br/>smoke tests"]) -.-> M1
```

Working now: the build on Linux, `ccsim --help` and `ccsim --version`. Everything else is planned.

## Go deeper

| Read | For |
|---|---|
| [TECHNICAL_DESIGN.md](TECHNICAL_DESIGN.md) | Exact rules, interfaces, OS details |
| [WORKING_DEMO.md](WORKING_DEMO.md) | Step-by-step examples with expected results |
| [DEVELOPMENT.md](DEVELOPMENT.md) | Build and test on your OS |
| [../CONTRIBUTING.md](../CONTRIBUTING.md) | Team roles and workflow |