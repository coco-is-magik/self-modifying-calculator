# Benchmark Revamp Plan

> **Date**: 2026-06-29
> **Purpose**: Comprehensive plan to fix benchmark methodology rigor, expand data coverage, and produce reliable, interpretable performance measurements.

---

## Table of Contents

1. [Current Problems](#1-current-problems)
2. [Tier 1 — Rigor Fixes](#2-tier-1--rigor-fixes)
3. [Tier 2 — Expanded Data Coverage](#3-tier-2--expanded-data-coverage)
4. [Tier 3 — New Benchmark Categories](#4-tier-3--new-benchmark-categories)
5. [Reporting Improvements](#5-reporting-improvements)
6. [File-by-File Implementation Plan](#6-file-by-file-implementation-plan)
7. [Implementation Phases](#7-implementation-phases)

---

## 1. Current Problems

### 1.1 Wall-clock timing with no GC isolation

`measure-time` in `benchmark-framework.lisp` uses `get-internal-real-time` (wall clock). No GC control is performed between benchmark phases. Allocation from the conventional evaluation pass carries forward into the cold SMC pass, and GC can trigger at unpredictable points in either phase, producing variance that has nothing to do with cache performance.

### 1.2 Single-run, single-seed measurements

Each (category × size) combination runs exactly once per `run-all-benchmarks` invocation, using a single random seed. The wide speedup ranges reported (e.g., dot product 2.3–8.0×) are artifacts of different runs hitting different repetition patterns in the random data.

### 1.3 Optimization levels mixed together

The current benchmark always enables Level 2 specialization (when `enable-specialization` is true), and the compiled dispatch (Level 1.5) is always active for small caches. This produces a "soup" of optimizations with no way to attribute speedup to a specific level. There is no way to answer "does caching alone help?" or "does specialization actually contribute?"

### 1.4 Specialization warmup pollutes cold-pass timing

`enable-arithmetic-specialization` is called once at the top of `run-all-benchmarks`, before any timing. The cold SMC pass then triggers specialization *during* the timed section as call thresholds are crossed. The recorded "cold" time includes runtime compilation overhead. The subsequent "warm" pass benefits from specializations installed mid-cold-pass, making both numbers impure.

### 1.5 No coverage of unique-expression ratio

The speedup numbers are reported without any information about how many unique ASTs appear in each series. A category with 27 unique expressions out of 100,000 (like dot product) is fundamentally different from one with 37,000 unique (like arithmetic). Without this ratio, cross-category comparisons are meaningless.

### 1.6 No cache-size bounding data

All benchmarks use an unlimited cache. There's no data on how LRU eviction affects performance — whether it helps arithmetic (by keeping only hot entries) or hurts categories with many unique expressions.

### 1.7 Cold-pass speedup hidden

Only warm-pass speedup is highlighted in the summary table. The cold pass is often *slower* than conventional (the overhead is real), but this is never reported. This matters for short-lived processes and one-off calculations.

---

## 2. Tier 1 — Rigor Fixes

### 2.1 CPU time instead of wall time

**File**: `tests/benchmarks/benchmark-framework.lisp`

Replace `get-internal-real-time` with `sb-ext:process-run-time`. CPU seconds eliminate noise from kernel scheduling, GC pauses, and other system activity. Trade-off: CPU time may undercount for allocation-bound workloads, but for pure computation this is strictly better.

```lisp
(defun measure-cpu-time (thunk)
  "Measure the CPU time taken by THUNK. Returns seconds as a float."
  (let ((start (sb-ext:process-run-time)))
    (funcall thunk)
    (let ((end (sb-ext:process-run-time)))
      (/ (- end start) internal-time-units-per-second))))
```

### 2.2 GC isolation between phases

**File**: `tests/benchmarks/benchmark-framework.lisp`

Before each timed section, call `(sb-ext:gc :full)` and then `(sleep 0.1)` to let the OS settle. Record GC time from `(sb-ext:gc-time)` and include it in the report.

```lisp
(defun gc-and-settle ()
  "Run a full GC and pause briefly to let the system settle."
  (sb-ext:gc :full)
  (sleep 0.1))
```

### 2.3 Multi-trial, multi-seed runs

**File**: `tests/benchmarks/benchmark-framework.lisp`

Run each (category × size × level × seed) combination as an independent trial. Report median + interquartile range or [min, max] across seeds 1–N (default N = 10). This directly addresses the noise problem.

```lisp
(defun run-trials (generator count level &key (seeds (loop for i from 1 to 10 collect i)))
  "Run N trials at different seeds and return a list of benchmark-result structs."
  (loop for seed in seeds
        collect (run-single-trial generator count level seed)))
```

### 2.4 Isolate optimization levels

**File**: `tests/benchmarks/run-all-benchmarks.lisp`

Create four benchmark modes and run them as separate passes:

| Mode | Cache | Compiled Dispatch | Specialization |
|------|-------|-------------------|----------------|
| **Baseline** | `evaluate-node` (no caching) | N/A | N/A |
| **Level 1** | `evaluate` with cache | Disabled | Disabled |
| **Level 1.5** | `evaluate` with cache | Enabled | Disabled |
| **Level 2** | `evaluate` with cache | Enabled | Enabled |

Report each separately. This tells you exactly which mechanism contributes the speedup.

### 2.5 Separate specialization warmup from timing

**File**: `tests/benchmarks/run-all-benchmarks.lisp`

Before timing anything, run the full series once with specialization tracking on. This allows patterns to cross the call-count threshold and installs specialized functions. Then clear the cache. Then time the conventional baseline, Level 1, Level 1.5, and Level 2 passes. This measures steady-state performance rather than cold-start + compilation overhead.

### 2.6 Pin SBCL optimization settings

**File**: `tests/benchmarks/benchmark-framework.lisp`

At the start of each benchmark run, set:

```lisp
(declaim (optimize (speed 3) (safety 1) (debug 0) (space 0)))
```

This ensures consistent compiler behavior across runs.

---

## 3. Tier 2 — Expanded Data Coverage

### 3.1 Report unique/total AST ratio per category

**File**: `tests/benchmarks/benchmark-framework.lisp`

For each series, compute:

```lisp
(/ (length (remove-duplicates series :test #'equal)) (length series))
```

Include this in every benchmark result. This single number is the best predictor of speedup. Current approximate ratios:

| Category | Unique/Total (LONG) |
|----------|--------------------|
| Arithmetic | ~37% |
| Dot Product | ~0.027% |
| Cross Product | ~0.05% |
| Trig | ~0.64% |
| Polynomial | ~0.08% |
| Mixed | ~12% |
| Realistic Renderer | ~0.05% |

### 3.2 Domain size sweep

**File**: `tests/benchmarks/series-generators.lisp`

Parameterize generators with domain width:

- `generate-arithmetic-asts` with operand range `[1..N]` for N = 5, 10, 20, 50, 100
- `generate-dot-product-asts` with component sets `{-1,0,1}`, `{-5..5}`, `{-50..50}`, continuous `[-1, 1]`
- `generate-trig-asts` with angle granularity: `N` evenly spaced points in `[0, π]`

Report speedup vs domain size to find the crossover point where cache overhead outweighs benefit.

### 3.3 Cache size bounding sweep

**File**: `tests/benchmarks/benchmark-framework.lisp`

Run each category with `max-size` = 100, 1000, 10000, unlimited. Tests whether LRU eviction helps by keeping only hot entries.

### 3.4 Cold-start reporting

**File**: `tests/benchmarks/benchmark-framework.lisp`

Add `cold-speedup` to the summary table alongside `warm-speedup`. Cold speedup = conventional time / SMC cold time. This is typically < 1.0 for arithmetic (SMC is slower on first pass) and > 1.0 for categories with high sub-expression reuse even on first pass.

---

## 4. Tier 3 — New Benchmark Categories

### 4.1 Matrix-vector multiplication (3D)

A 4×4 matrix × vec3 is ~12 multiplications + 9 additions per vertex. Core 3D graphics operation. Two variants:

- **Same matrix, different vectors**: e.g., one view matrix applied to many vertices
- **Different matrices, same vector**: e.g., one vertex transformed by many bone matrices

```lisp
(defun generate-matrix-vector-asts (count &optional (seed 42))
  "Generate COUNT matrix-vector multiply ASTs.
   Variant: same matrix, random vectors."
  ...)
```

### 4.2 String-parsed end-to-end

Currently all benchmarks bypass `parse` by constructing ASTs directly via `make-ast`. A full parse+eval benchmark measures parser overhead and parse-cache benefit. Important because `tokenize` allocates fresh lists on every call.

```lisp
(defun generate-expression-strings (count &optional (seed 42))
  "Return a list of COUNTS strings, not ASTs."
  (let ((asts (generate-arithmetic-asts count seed)))
    (mapcar #'ast-to-string asts)))
```

### 4.3 Blinn-Phong shading

A complete lighting model that exercises multiple operators sharing sub-expressions:

```
ambient + diffuse * dot(N, L) + specular * dot(N, H)^shininess
```

More realistic than the current renderer benchmark (which is just `dot(N,L) + sin(angle)`).

### 4.4 Cross-session persistence benchmark

Measure Level 3 benefit:

1. Fresh process, no cache — time a category
2. Same process, Level 2 enabled — populate cache
3. Save cache via Level 3 source rewrite
4. Fresh process, load pre-populated cache — time same category

This quantifies the benefit of persistent optimization.

---

## 5. Reporting Improvements

### 5.1 New benchmark result fields

| Field | Source | Purpose |
|-------|--------|---------|
| `unique-ratio` | `(remove-duplicates series :test #'equal)` | Predictor of cache effectiveness |
| `gc-time` | `sb-ext:gc-time` | Distinguish "real speedup" from "less GC pressure" |
| `cold-speedup` | `conventional-time / smc-cold-time` | First-pass performance |
| `warm-speedup` | `conventional-time / smc-warm-time` | Steady-state performance |
| `level` | `:level` parameter | Which optimization level was active |

### 5.2 Summary table format

```
=== Benchmark Summary (Level 2, 10 trials) ===
───────────────────────────────────────────────────────────────
Category         Count    Unique%  Conv(ms)  Warm(ms)  Speedup  (range)
───────────────────────────────────────────────────────────────
Arithmetic       100000   37.1%    0.0009    0.0008    1.15×    [0.98, 1.21]
Dot Product      100000    0.03%   0.0082    0.0016    5.12×    [4.81, 5.43]
...
───────────────────────────────────────────────────────────────
```

### 5.3 Per-level comparison

After running at all levels, print a comparative table:

```
=== Optimization Level Comparison (LONG, 100k) ===
─────────────────────────────────────────────────
Category         L1       L1.5     L2
─────────────────────────────────────────────────
Arithmetic       1.02×    1.05×    1.15×
Dot Product      3.10×    3.90×    5.12×
...
─────────────────────────────────────────────────
```

This is the most useful output: it shows which optimization mechanism contributes the most for each category.

---

## 6. File-by-File Implementation Plan

### 6.1 `tests/benchmarks/benchmark-framework.lisp`

**Changes**:
- Add `*optimization-configs*` alist mapping level keywords to configuration callbacks
- Add `*benchmark-seeds*` configurable list of seeds (default `(loop for i from 1 to 10 collect i)`)
- Add `measure-cpu-time` using `sb-ext:process-run-time`
- Add `gc-and-settle` helper
- Add `unique-ratio` field to `benchmark-result`
- Add `cold-speedup` and `warm-speedup` computed fields
- Add `level` field to `benchmark-result`
- Add `run-single-trial` that handles per-level configuration
- Add `run-trials` that iterates over seeds and aggregates results
- Add `print-benchmark-with-stats` that shows median + range
- Add `print-level-comparison` that produces per-level summary
- Remove old `run-benchmark-category` or refactor into `run-single-trial`

### 6.2 `tests/benchmarks/run-all-benchmarks.lisp`

**Changes**:
- Accept `:level` parameter (one of `:all`, `:baseline`, `:l1`, `:l1.5`, `:l2`)
- Accept `:trial-count` parameter (default 10)
- Accept `:domain-sizes` parameter for sweeps
- Add `run-level` function that configures the evaluator, runs warmup, then runs trials
- Add `run-domain-sweep` that iterates over domain sizes
- Add `run-cache-size-sweep` that iterates over max-size values
- Call `gc-and-settle` between each phase
- Call `run-collect-stats` after each set of trials

### 6.3 `tests/benchmarks/series-generators.lisp`

**Changes**:
- Add `domain-size` parameter to `generate-arithmetic-asts` (controls operand range)
- Add `component-set` parameter to `generate-dot-product-asts` and `generate-cross-product-asts`
- Add `angle-count` parameter to `generate-trig-asts`
- Add `generate-matrix-multiply-asts` (new category)
- Add `generate-blinn-phong-asts` (new category)
- Add `generate-expression-strings` (for end-to-end parse+eval benchmark)

### 6.4 `scripts/run-benchmarks.sh`

**Changes**:
- Add `--level` flag (baseline/l1/l1.5/l2/all)
- Add `--trials` flag (number of seeds to try)
- Add `--sweep-domain` flag (run domain size sweep)
- Add `--sweep-cache` flag (run cache size sweep)
- Add `--output` flag for results file
- Default: run all levels at 10 trials each

### 6.5 `tests/benchmarks/benchmark-config.lisp` (new file)

**Purpose**: Centralize configuration constants so benchmark framework, series generators, and scripts share the same defaults.

Contents:
- Default seed range
- Default series lengths
- Optimization level callbacks
- SBCL optimization proclamations

---

## 7. Implementation Phases

### Phase A — Core Rigor (estimated: 1 session)

| Step | File | Description |
|------|------|-------------|
| A1 | `benchmark-framework.lisp` | Add `measure-cpu-time`, `gc-and-settle` |
| A2 | `benchmark-framework.lisp` | Add multi-trial infrastructure: `run-single-trial`, `run-trials` |
| A3 | `benchmark-framework.lisp` | Add `unique-ratio` and `level` to `benchmark-result` |
| A4 | `benchmark-framework.lisp` | Add median + range reporting |
| A5 | `run-all-benchmarks.lisp` | Add optimization level parameter, configure evaluator accordingly |
| A6 | `run-all-benchmarks.lisp` | Add warmup pass before timed sections |
| A7 | `run-all-benchmarks.lisp` | Add per-level comparison table output |
| A8 | `scripts/run-benchmarks.sh` | Update to pass new flags |

**Outcome**: Benchmarks are rigorous, reproducible, and attribute speedup to specific optimization levels.

### Phase B — Expanded Coverage (estimated: 1 session)

| Step | File | Description |
|------|------|-------------|
| B1 | `series-generators.lisp` | Add `domain-size` / `component-set` parameters to existing generators |
| B2 | `benchmark-framework.lisp` | Add domain size sweep runner |
| B3 | `benchmark-framework.lisp` | Add cache size sweep runner |
| B4 | `run-all-benchmarks.lisp` | Add sweep entries |
| B5 | `scripts/run-benchmarks.sh` | Add sweep flags |

**Outcome**: Data shows how performance degrades with larger domains and how LRU eviction affects each category.

### Phase C — New Categories (estimated: 1–2 sessions)

| Step | File | Description |
|------|------|-------------|
| C1 | `series-generators.lisp` | Add `generate-matrix-multiply-asts` |
| C2 | `series-generators.lisp` | Add `generate-blinn-phong-asts` |
| C3 | `series-generators.lisp` | Add `generate-expression-strings` for end-to-end |
| C4 | `run-all-benchmarks.lisp` | Register new categories |
| C5 | `benchmark-framework.lisp` | Add end-to-end benchmark mode (string→parse→eval) |

**Outcome**: Coverage includes real 3D math workloads and the full parse+eval pipeline.

### Phase D — Reporting Polish (estimated: 1 session)

| Step | File | Description |
|------|------|-------------|
| D1 | `benchmark-framework.lisp` | Add `print-level-comparison` |
| D2 | `scripts/run-benchmarks.sh` | Add `--output` flag for machine-readable JSON/sexp output |
| D3 | `README.md` | Update benchmark results section referencing new methodology |
| D4 | `docs/notes/performance-profiling.md` | Add new benchmark results with ranges |

**Outcome**: Comprehensive, interpretable reports suitable for documentation and comparison across sessions.

---

*This document is the implementation plan for the benchmark revamp. Update it as the implementation evolves and as new measurement needs arise.*