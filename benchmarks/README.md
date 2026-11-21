# Benchmarks

Standalone harness for performance measurements. Not part of CTest.

```powershell
# Windows
scripts/bench.ps1
scripts/bench.ps1 --iterations 5000 --json benchmarks/results/latest.json
scripts/bench.ps1 --check benchmarks/baseline.json --max-regression 15
```

```sh
# Linux
./scripts/bench.sh
./scripts/bench.sh --iterations 5000 --json benchmarks/results/latest.json
./scripts/bench.sh --check benchmarks/baseline-linux.json --max-regression 15
```

Baselines are host-specific. `baseline.json` is the Windows reference;
`baseline-linux.json` is the Linux reference.

Add a `*.cpp` file, include it in `benchmarks/CMakeLists.txt`, and use
`BENCHMARK_CASE(name, ops_per_call)`. See `docs/benchmarks.md`.
