# Session 4 Implementation Notes

> **Purpose**: This document records the incremental implementation of the
> Session 4 plan (compiled cache dispatch + vector math module + refactored
> benchmark suite). It captures the approach taken at each step, why it was
> chosen, and what did or did not work.
>
> **How to read**: Entries are ordered chronologically. Each entry represents
> one iterative step in the implementation.

## Step 3 — Vector / Linear Algebra Module

**Approach**:
- Created `src/math/linear-algebra.lisp` with operators `:vec3`, `:dot`, `:cross`, `:norm`, `:normalize`.
- Vectors are represented as 3-element lists to keep them cache-friendly and printable.
- Added `src/math/trigonometry.lisp` with `:sin` and `:cos` operators for the trig benchmark category.
- Updated `self-modifying-calculator.asd` to load both new modules.
- Added `tests/core/test-linear-algebra.lisp` with 5 test groups.
- Modified `make-ast` to wrap raw numeric literals in constant nodes automatically, so `make-ast :vec3 0 0 1` works without manual `constant-node` calls.

**What worked**:
- All 38 tests pass, including the new vector tests.
- Dot-product expressions evaluate correctly, e.g., `(dot (vec3 0 0 1) (vec3 0.6 0 0.8))` returns `0.8`.

---

## Step 4 — Refactored Benchmark Suite

**Approach**:
- Created `tests/benchmarks/benchmark-framework.lisp` with `benchmark-result`, `measure-time`, `run-benchmark-category`, `print-benchmark-result`, and `print-summary-table` (later replaced by `benchmark-trial` / `trial-aggregate` and `run-single-trial` in the Session 6 benchmark overhaul).
- Created `tests/benchmarks/series-generators.lisp` with deterministic generators for:
  - Arithmetic
  - Dot product
  - Cross product
  - Trigonometry
  - Polynomial
  - Mixed
  - Realistic renderer
- Created `tests/benchmarks/run-all-benchmarks.lisp` as the entry point that runs all categories at short/medium/long lengths and prints a summary table.
- Each benchmark evaluates the same AST series three times: conventional (uncached), SMC cold, SMC warm.

**Results after Step 5 (remove rewrite step)**:

| Category (LONG) | Conventional | SMC cold | SMC warm | Speedup |
|---|---|---|---|---|
| Arithmetic | 0.0233 s | 0.0967 s | 0.0200 s | **1.17×** |
| Dot Product | 0.0333 s | 0.0100 s | 0.0100 s | **3.33×** |
| Cross Product | 0.0300 s | 0.0100 s | 0.0100 s | **3.00×** |
| Trig | 0.0200 s | 0.0100 s | 0.0167 s | **1.20×** |
| Polynomial | 0.0467 s | 0.0133 s | 0.0133 s | **3.50×** |
| Mixed | 0.0267 s | 0.0333 s | 0.0100 s | **2.67×** |
| Realistic Renderer | 0.0433 s | 0.0133 s | 0.0133 s | **3.25×** |

- Short series timings are too fast to measure accurately (0.0000 s).
- Medium series is partially measurable but still noisy.
- Long series clearly shows the calculator wins on all realistic workloads (dot, cross, polynomial, mixed, realistic renderer). Arithmetic, as expected, is the hardest case because the operations are so cheap.

---

## Step 5 — Removed Explicit `rewrite-with-cache` from Hot Path

**Approach**:
- Changed `evaluate-node-uncached` to evaluate children directly via `evaluate-node-cached` instead of first rewriting the AST with cached sub-expressions.
- This avoids constructing a new AST on every cache miss.
- Cached sub-trees are still hit because each child goes through `evaluate-node-cached`.

**Why this worked**:
- Rewriting the AST on every miss added overhead without benefit when most sub-trees were also misses.
- The new path is simpler and faster.
- Cache reuse is preserved because the recursive evaluator still checks the cache for every node.

**Results**:
- Arithmetic long series improved from 0.38× to **1.17×**.
- All other categories improved or stayed strong.
- All 38 tests still pass.

**Trade-offs**:
- The `rewrite-with-cache` / `matcher.lisp` code is no longer used in the hot path, but it remains in the source tree for potential future use (e.g., when we want to rewrite source code before compilation).

---

## Current Status and Next Steps

**Completed**:
- [x] Canonical AST interning + EQ hash table (core performance win)
- [x] Vector / linear algebra module
- [x] Trigonometry module
- [x] Refactored benchmark suite with 7 categories
- [x] Removed explicit rewrite overhead
- [x] All 38 tests pass

**Remaining / Future Work**:
- [ ] Improve short/medium benchmark timing accuracy (e.g., run multiple passes and accumulate time).
- [ ] Revisit the compiled dispatch table with a smarter data structure (e.g., trie, hash-based dispatch) that avoids the linear-scan/stack-overflow problem.
- [ ] Consider an LRU or size-bound eviction policy for the cache.
- [ ] Add more math modules (algebra, calculus, statistics) as planned.
- [ ] Update `docs/plan.md`, `docs/HANDOFF.md`, `docs/CHANGELOG.md`, and `README.md` to reflect the new architecture and benchmark results.


## Step 0 — Starting Point

**Status before Session 4**:
- Phases 1–3 are complete and tested (33 tests pass).
- The cache-aware evaluator uses a generic `EQUAL` hash table on AST lists.
- Benchmarks show the cache is slower for simple arithmetic and only ~1.20× faster for dot products.
- Documentation in `docs/plan.md`, `docs/HANDOFF.md`, and `README.md` has been updated with the A + D corrective plan.

**Selected approach**:
1. Implement compiled cache dispatch table.
2. Add vector/linear algebra module.
3. Refactor benchmark suite into per-category benchmarks.
4. Record results and iterate.

---

## Step 1 — Compiled Cache Dispatch (Attempted, Then Disabled for Large Caches)

**Approach**:
- Added `compiled-lookup`, `compiled-lookup-dirty`, `compile-threshold`, and `last-compiled-size` slots to the `cache` struct.
- Created `src/core/dispatch-compiler.lisp` to generate a compiled `cond` function from the cache entries.
- Modified `src/core/evaluator.lisp` to check the compiled lookup before the generic hash table.
- Used a doubling strategy (recompile when cache size reaches threshold or doubles) to avoid recompiling on every new entry.

**What worked**:
- For small caches the compiled dispatch returned correct values.
- All 33 tests still passed after integration (once the `:not-found` return value bug was fixed to be `nil` instead of a truthy symbol).

**What failed**:
- For medium/large caches, the compiled function became a linear scan of thousands of clauses. Each clause performed a recursive `ast-equal-p` structural comparison, so lookup was actually **slower** than the hash table.
- The long series (~37,000 unique expressions) caused a **control stack exhausted** error during compilation because SBCL could not handle a `cond` with tens of thousands of clauses.
- The cold pass became very slow (0.95–7.76 seconds for 10k calculations) because of repeated compilation.

**Decision**: Disabled the compiled dispatch from the hot path. The file remains in the source tree for future refinement, but the evaluator now uses only the EQ hash table.

---

## Step 2 — Canonical AST Interning + EQ Hash Table

**Approach**:
- Added `*ast-intern-table*` in `src/core/ast.lisp` with an `EQUAL` test.
- Modified `constant-node`, `variable-node`, and `make-ast` to return canonical interned objects.
- Changed the cache hash table from `EQUAL` to `EQ`.
- Made `cache-set` intern its key so that cache entries loaded from files also become canonical objects.

**Why this worked**:
- `EQ` pointer comparison is much faster than `EQUAL` structural comparison for hash lookup.
- Because the parser returns canonical AST objects, identical expressions map to the same cache bucket without re-computing the hash of the list structure.
- The rewrite-with-cache path still works because all ASTs are canonical; replacing a cached sub-tree with a constant node produces a canonical operator node.

**Results (arithmetic benchmark)**:

| Series | Conventional | SMC cold | SMC warm | Speedup |
|---|---|---|---|---|
| Short (100) | 0.0000 s | 0.0000 s | 0.0000 s | INFx |
| Medium (10k) | 0.0167 s | 0.0100 s | 0.0033 s | **5.00×** |
| Long (100k) | 0.1733 s | 0.1367 s | 0.1200 s | **1.44×** |

- Medium series now exceeds the plan target of 2.0×.
- Long series is faster than conventional but still below the 5.0× target. The long series has ~37,303 unique whole expressions out of 100,000, so only ~63% are cache hits.
- The short series is too fast to measure accurately.

**Status**: This is the current winning optimization. The compiled dispatch is disabled for now.

---
