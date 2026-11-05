# 0004 — Benchmarks separate from tests

## Context

Performance cases ran inside the unit-test executable and printed timings with
no baseline or regression check. They slowed normal test runs and mixed
correctness with measurement.

## Decision

Build `raywaves_benchmarks` as a standalone executable. It is not registered
with CTest. `scripts/bench.ps1` runs it, writes JSON, and can compare against
`benchmarks/baseline.json`.

## Consequences

- Unit tests stay fast and deterministic.
- Benchmarks run only when requested.
- Timing checks are machine-dependent; use Release builds and quiet machines.
- Benchmark cases use `BENCHMARK_CASE(name, ops_per_call)`.
