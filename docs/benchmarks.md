# Benchmarks

`raywaves_benchmarks` is a standalone harness. It runs warm-ups, then timed
iterations, and reports nanoseconds per operation and operations per second.

## Running

```powershell
# Default: release build, all cases
scripts/bench.ps1

# Custom iterations
scripts/bench.ps1 --iterations 5000 --warmup 500

# Filter by case name
scripts/bench.ps1 --filter Profiler

# Write a JSON report
scripts/bench.ps1 --json benchmarks/results/latest.json
```

The executable is `build/<preset>/raywaves_benchmarks.exe`; it is not part of
CTest and never runs as part of `scripts/test.ps1`.

## Comparing against a baseline

```powershell
scripts/bench.ps1 --check benchmarks/baseline.json --max-regression 10
```

The check is deliberately simple: it reads the flat JSON written by the
harness and fails when a case is more than `--max-regression` percent slower.
Run it on a quiet machine; timing noise is expected on laptops.

## Adding a case

Add a `.cpp` file under `benchmarks/` and include it in
`benchmarks/CMakeLists.txt`:

```cpp
#include "support/bench.hpp"

BENCHMARK_CASE("MySystem::Update", 1000)
{
    for (int i = 0; i < 1000; ++i)
    {
        // one operation
    }
}
```

`ops_per_call` tells the harness how many operations the body performs so the
reported cost is per operation, not per function call. Benchmarks should use
Release builds; debug numbers are only useful for relative comparisons.
