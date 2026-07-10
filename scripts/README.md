# Scripts

This folder contains convenience scripts for running the Self-Modifying Calculator.

## Available scripts

| Script | Purpose | Example |
|---|---|---|
| `run-calculator.sh` | Evaluate one or more expressions from the command line | `./scripts/run-calculator.sh "2+3*4"` |
| `run.sh` | Backward-compatible alias for `run-calculator.sh` | `./scripts/run.sh "2+3*4"` |
| `run-tests.sh` | Run the full test suite | `./scripts/run-tests.sh` |
| `run-benchmarks.sh` | Run the full Lisp benchmark suite | `./scripts/run-benchmarks.sh` |
| `run-demo.sh` | Run the focused realistic-renderer demo | `./scripts/run-demo.sh` |
| `run-linter.sh` | Load the system with stricter settings to surface warnings | `./scripts/run-linter.sh` |

## C Benchmarks

The C benchmarks are built with `cmake -B build && cmake --build build`:

```bash
./build/benchmark_indexed_state  # Measure generic vs indexed vs batch state APIs
```

This benchmark simulates renderer-like workloads with 41,600 cells (matching terminal-style renderers) and reports nanoseconds-per-operation for:
- Generic state unchanged/changed paths
- Indexed state unchanged/changed paths  
- Batch indexed unchanged/changed paths

## Notes

- All scripts change to the project root before invoking SBCL, so they can be run from any directory.
- They assume `sbcl` is available on your `PATH`.
- The demo and benchmark scripts rely on files in `tests/benchmarks/`.