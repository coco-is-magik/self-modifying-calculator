# Performance Profiling Notes

> **Date**: 2026-06-30
> **Purpose**: Record the findings and decisions from the performance tuning session aimed at reaching the 5×/20× speedup targets.

## Benchmark Methodology

The benchmark suite now uses a rigorous, reproducible methodology:

- **CPU time** is measured via `get-internal-run-time` (not wall time), excluding kernel scheduling noise.
- **GC isolation**: a full GC and short settle period precede each timed phase.
- **Multi-trial aggregation**: each (category × size × optimization level) combination is run across multiple random seeds. Results report the **median** warm speedup plus a `[min, max]` range.
- **Optimization levels are isolated**:
  - `BASELINE`: raw `evaluate-node`, no caching.
  - `L1`: EQ hash-table cache only, no compiled dispatch, no operator specialization.
  - `L1.5`: cache + compiled dispatch.
  - `L2`: cache + compiled dispatch + operator specialization (with a separate warmup pass so compilation cost is not timed).
- **Unique-ratio reporting**: the fraction of unique ASTs in each series is recorded, because it is the strongest predictor of cache effectiveness.

Run benchmarks with:

```bash
./scripts/run-benchmarks.sh              # all levels, 10 trials, output to /tmp
./scripts/run-benchmarks.sh --level l2   # single level
./scripts/run-benchmarks.sh --trials 3   # faster, fewer trials
```

Full output is written to `/tmp/smc-benchmark-<timestamp>.out`; only `[BENCH]` progress lines appear on the terminal. The process runs at `nice -n 19` by default to keep the machine responsive.

## Categories

The suite covers:

| Category | What it tests |
|---|---|
| Arithmetic | Cheap scalar ops (`+`, `-`, `*`) with varying operand domains |
| Dot Product | `dot(N, L)` with a fixed light vector and random normals |
| Cross Product | `cross(v1, v2)` with random vectors |
| Trig | `sin(a) + cos(b)` with a controlled set of angles |
| Polynomial | `a*x^2 + b*x + c` with shared coefficients/inputs |
| Mixed | Random mix of arithmetic, dot, trig, and polynomial ASTs |
| Realistic Renderer | `dot(N,L) * sin(angle) + ambient` — the priority target |
| Matrix Multiply | `mat4x4-mul(matrix, vector)` — fixed matrix, random vectors |
| Blinn-Phong | `ambient + diffuse*dot(N,L) + specular*dot(N,H)^shininess` |
| End-to-End Parse+Eval | Full `parse` → `evaluate` pipeline including parser overhead |

## Baseline (before tuning)

| Category (LONG, 100k) | Speedup |
|---|---|
| Arithmetic | 0.71× |
| Dot Product | 2.90× |
| Cross Product | 2.29× |
| Trig | 1.18× |
| Polynomial | 1.00× |
| Mixed | 1.09× |
| Realistic Renderer | 2.56× |

## Findings from `sb-sprof`

Profiling the realistic-renderer and mixed warm paths revealed that a significant amount of time was spent creating a fresh `*variable-table*` hash table on every call to `evaluate`, even when no variables were bound. Each top-level call created a new hash table, which showed up as `SB-IMPL::%MAKE-HASH-TABLE` and allocation overhead in the profile.

Other observations:
- LRU bookkeeping in `cache-touch` runs on every access, even for unlimited caches where it is unnecessary.
- Constants and variables go through the same cache lookup path as operator nodes, adding cache-table probes for nodes that are trivially known.

## Changes made

1. **Avoid variable-table allocation when no variables are provided**
   - `evaluate` now only creates a hash table when the `variables` argument is non-nil.
   - `variable-value` handles the nil table gracefully.

2. **Conditional LRU bookkeeping**
   - `cache-touch` only updates the access counter when `max-size > 0`.
   - Unlimited caches no longer pay the LRU overhead.

3. **Fast path for constants and variables**
   - `evaluate-node-cached` returns constant values and variable values directly, bypassing the cache lookup entirely.

4. **Level 2 specialization enabled in benchmark runs**
   - `run-all-benchmarks` now enables operator specialization for `+`, `*`, `-`, `/` by default.
   - Fixed `generate-specialized-function` to emit a `&rest` lambda so variadic operators (e.g., `+` with 3 args in polynomial expressions) do not error.

5. **Compiled dispatch limit tuning**
   - Tried raising `*compiled-dispatch-max-clauses*` to 500.
   - Result was worse for realistic renderer (330 entries) because a linear scan of ~300 `eq` clauses is slower than an EQ hash-table lookup.
   - Reverted to 100, which is optimal for the small caches (dot/cross/trig) that benefit from it.

6. **Benchmark rigor overhaul**
   - Switched from wall time to CPU time.
   - Added GC isolation between phases.
   - Added multi-trial aggregation with median + range.
   - Separated optimization levels so each is measured independently.
   - Added progress output and `nice -n 19` in the runner script.
   - Added machine-readable sexp output for analysis.

## Results after tuning (representative LONG runs)

| Category | Speedup range (LONG) | Notes |
|---|---|---|
| Arithmetic | 0.9–1.9× | Still below target; operations are too cheap to amortize cache overhead. |
| Dot Product | 2.3–8.0× | Very noisy; often above 5×. |
| Cross Product | 2.3–8.3× | Often above 5×. |
| Trig | 2.3–5.4× | Usually 3–4×. |
| Polynomial | 1.8–8.6× | Often above 5×. |
| Mixed | 2.0–5.2× | Sometimes reaches 5×. |
| Realistic Renderer | 5.0–8.0× | Consistently exceeds the 5× minimum target. |
| Matrix Multiply | 2.5–8.0× | High sub-expression reuse across matrix rows. |
| Blinn-Phong | 3.5–5.5× | Multiple shared sub-expressions; good cache reuse. |

The priority category (realistic renderer) now meets the 5× minimum target. The remaining gap is mainly in arithmetic, which is the hardest case because the operations themselves are extremely cheap.

## Sweeps and analysis tools

- `smc:run-domain-sweep` — vary operand/component domains to see how unique-ratio affects speedup.
- `smc:run-cache-size-sweep` — measure speedup at bounded cache sizes (100, 1k, 10k, unlimited).
- Machine-readable output is printed at the end of every `run-all-benchmarks` invocation as a sexp under `(benchmark-results ...)`.

## Next options for further improvement

- **Per-operator hash dispatch**: replace the linear `cond` compiled dispatch with a `case` on the operator symbol plus a per-operator hash table. This could give O(1) lookup for caches too large for the current linear dispatch (e.g., realistic renderer, mixed) without falling back to the general EQ hash table.
- **Selective caching**: skip caching whole arithmetic expressions or very small ASTs to reduce cache growth and lookup overhead, while keeping sub-expression sharing.
- **Bounded cache**: test whether a size-bound cache keeps the hottest entries and improves lookup time for categories with many unique entries (arithmetic, mixed).
- **Inline/cache the operator table**: `operator-function` does a hash lookup for every operator call; pre-binding the operator functions in the evaluator could reduce per-call overhead.
