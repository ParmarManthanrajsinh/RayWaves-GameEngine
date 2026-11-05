# Benchmarks

Standalone harness for performance measurements. Not part of CTest.

```powershell
scripts/bench.ps1
scripts/bench.ps1 --iterations 5000 --json benchmarks/results/latest.json
scripts/bench.ps1 --check benchmarks/baseline.json
```

Add a `*.cpp` file, include it in `benchmarks/CMakeLists.txt`, and use
`BENCHMARK_CASE(name, ops_per_call)`. See `docs/benchmarks.md`.
