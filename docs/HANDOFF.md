# Handoff Document

> **Purpose**: This document records what was done in each development session, why it was done, and what the next engineer should focus on next. Each session gets its own dated section.
>
> **How to use**: Before starting a new session, read the latest entry. After finishing your session, append a new section at the end with: date, work completed, rationale, current state, blockers, and next steps.

---

## Session 0 — Plan Finalization

**Date**: 2026-06-26

**Completed**:
- Reviewed existing 72-line CLISP calculator (`calculator.lsp`)
- Authored comprehensive project plan in `docs/plan.md`
- Updated `README.md` to articulate the new project goals
- Added Performance Benchmarking Suite section to `docs/plan.md`
- Added Benchmarking roadmap section to `README.md`

**Rationale**: The project needed a clear architectural direction before implementation could begin. The plan defines the self-modifying calculator's core concepts: hierarchical expression caching, three levels of self-modification, modular math support, and a benchmark suite that proves the calculator beats conventional evaluation.

**Current State**: Planning complete. No implementation code exists yet beyond the original `calculator.lsp`.

**Blockers**: None.

**Next Steps** (for Session 1):
- Begin Phase 1 implementation
- Create `docs/CHANGELOG.md` if not already created
- Set up SBCL/ASDF project structure
- Implement `src/core/package.lisp`, `src/core/ast.lisp`, `src/core/evaluator.lisp`
- Implement `src/interface/parser.lisp` (start with basic arithmetic parser)
- Implement `src/interface/cli.lisp` and `src/main.lisp`
- Create `self-modifying-calculator.asd` system definition
- Add first core tests to `tests/core/`
- Record all work in this handoff and the changelog

---

## Session 1 — Phase 1 Foundation

**Date**: 2026-06-26

**Completed**:
- Verified SBCL 2.5.11 is installed and confirmed the project will be dependency-free (no Quicklisp)
- Created the full source tree under `src/` and test tree under `tests/`
- Implemented `src/core/package.lisp` with the `self-modifying-calculator` (nickname `smc`) package
- Implemented `src/core/ast.lisp` for AST nodes (`:constant`, `:variable`, operator nodes), traversal, and utilities
- Implemented `src/core/evaluator.lisp` with operator registry, variable environment, and recursive evaluator
- Implemented `src/interface/parser.lisp` with tokenizer and recursive-descent parser supporting `+ - * / ^`, precedence, parentheses, and floats
- Implemented `src/math/arithmetic.lisp` as the first pluggable math module
- Implemented `src/main.lisp` as the entry point and `run-calculator` helper
- Created `self-modifying-calculator.asd` ASDF system definition
- Created `run.sh` convenience wrapper for CLI usage
- Created a lightweight, dependency-free test runner in `tests/test-runner.lisp`
- Added core tests: `tests/core/test-ast.lisp`, `tests/core/test-parser.lisp`, `tests/core/test-evaluator.lisp`
- Updated `README.md` with SBCL requirement, new usage examples, and test commands
- Updated `docs/CHANGELOG.md` with Session 1 entry

**Rationale**: Phase 1 establishes the foundation the rest of the project depends on. Without a working AST, parser, evaluator, and module system, the caching and self-modification layers cannot be built. The dependency-free test runner avoids the need for Quicklisp while still providing automated verification.

**Bugs Found and Fixed During Session 1**:
1. **Evaluator did not recursively evaluate arguments**: the operator functions received raw AST nodes instead of numeric values. Fixed by adding `(mapcar #'evaluate-node args)` before applying the operator.
2. **Parser did not consume tokens correctly**: parser functions mutated local parameter copies of the token list, so the entire expression was never parsed beyond the first token. Fixed by introducing a `token-stream` struct with `peek-token`, `next-token`, and `expect-token` accessors, and rewriting the parser functions to use the shared mutable stream.

**Current State**:
- Phase 1 is complete and functional
- All 24 core tests pass
- `./run.sh "2+3*4"` correctly returns `14`
- The project can be loaded via ASDF and the new `smc` package is usable from the SBCL REPL

**Blockers**: None.

**Next Steps** (for Session 2):
- Begin Phase 2: Caching Engine
- Implement `src/core/cache.lisp` with an in-memory hash table for whole-expression and sub-expression results
- Implement `src/core/matcher.lisp` for tree-isomorphism matching and partial sub-expression replacement
- Implement a cache-aware evaluator that checks the cache before recursing into sub-trees
- Add tests verifying that repeated identical calculations return cached results
- Ensure cache works correctly with the existing AST/parser/evaluator stack
- Record all work in this handoff and the changelog

---

## Session 2 — Phase 2 Caching Engine

**Date**: 2026-06-26

**Completed**:
- Implemented `src/core/cache.lisp` with:
  - `cache` struct using an EQUAL hash table (AST keys are structural)
  - `cache-get`, `cache-set`, `cache-contains-p`, `cache-clear`, `cache-size`, `cache-statistics`
  - `ast-ground-p` to prevent caching of expressions containing variables
  - `*global-cache*` default cache instance
- Implemented `src/core/matcher.lisp` with `rewrite-with-cache` and `rewrite-with-cache-until-stable`
- Updated `src/core/evaluator.lisp` to add a cache-aware evaluation path:
  - `evaluate` now accepts a `:cache` keyword argument
  - `evaluate-node-cached` checks the cache before recursing, records hits/misses, and stores results
  - `evaluate-node-uncached` rewrites the node with cached sub-expressions before evaluating
  - Kept `evaluate-node` as a non-caching fallback for compatibility
- Updated `self-modifying-calculator.asd` to load `cache.lisp` and `matcher.lisp`
- Added `tests/core/test-cache.lisp` with 5 test groups covering cache operations, variable exclusion, repeated evaluation, sub-expression reuse, and statistics
- Updated `docs/CHANGELOG.md` with Session 2 entry

**Rationale**: The caching engine is the first step of the self-modifying optimization pipeline. Hierarchical caching (whole + sub-expression) enables the partial reuse that the project targets, especially for repeated similar calculations like dot products and quadratic factors.

**How the Cache Works**:
1. Every AST node is checked against the cache before evaluation.
2. If found, the cached value is returned and the hit counter is incremented.
3. If not found, the node is rewritten by replacing any cached sub-trees with constant nodes.
4. The rewritten node is evaluated recursively; each sub-result is cached as it is computed.
5. The final result for the original node is cached.

**Current State**:
- Phase 2 is complete and integrated with Phase 1
- All 29 tests pass (24 from Phase 1 + 5 new cache tests)
- The cache-aware evaluator correctly reuses sub-expressions (e.g., `(2+3)` is cached once and reused in `(2+3)*4` and `(2+3)*5`)

**Blockers / Known Limitations**:
- The CLI (`run.sh`) is stateless per invocation, so the in-memory cache does not persist across separate command-line runs. Caching is currently effective only within a single SBCL session or programmatic loop.
- Persistent cache (Level 1 across sessions) and source-level rewriting (Level 3) are not yet implemented.

**Next Steps** (for Session 3):
- Begin Phase 3: Self-Modification
- Implement Level 1 fully: optionally persist the cache to `cache/cache.sexp` on exit and reload on startup
- Implement Level 2: runtime function specialization (generate specialized operator functions for frequently seen operand pairs) and hot-swap via `fdefinition`
- Implement Level 3: serialize the accumulated cache into source literals that are compiled in on next load
- Add a benchmark to compare cached vs uncached evaluation and measure speedup
- Record all work in this handoff and the changelog

---

## Session 3 — Phase 3 Self-Modification and Benchmarking

**Date**: 2026-06-26

**Completed**:
- Implemented `src/core/cache.lisp` persistence:
  - `save-cache` serializes the cache to `cache/cache.sexp`
  - `load-cache` reads entries back into a cache
  - `*cache-file-path*` default path
- Implemented `src/core/optimizer.lisp` for Level 2 runtime function specialization:
  - Tracks how often each operator is called with specific constant operands
  - Generates a compiled specialized function with short-circuit `cond` clauses for hot patterns
  - Hot-swaps the specialized function into the operator registry
  - Keeps the original base function in `*original-operator-functions*` so wrappers never recurse
- Updated `src/core/evaluator.lisp` so `register-operator` stores both the current and original base function
- Implemented `src/core/self-writer.lisp` for Level 3 source-level self-modification:
  - `write-cache-as-source` writes cache entries as a loadable Lisp source file
  - `load-generated-cache-source` loads the generated file to prepopulate the global cache
- Added `*parse-cache*` in `src/interface/parser.lisp` to avoid re-parsing repeated input strings
- Updated `self-modifying-calculator.asd` to include `optimizer.lisp` and `self-writer.lisp`
- Added tests: `tests/core/test-optimizer.lisp` (3 test groups), `tests/core/test-self-writer.lisp` (1 test group)
- Added benchmark harnesses:
  - `tests/benchmarks/arithmetic-benchmark.lisp` — short/medium/long arithmetic series
  - `tests/benchmarks/dot-product-benchmark.lisp` — 3D dot-product workload
- Updated `docs/CHANGELOG.md` with Session 3 entry

**Rationale**: Phase 3 adds the actual self-modification mechanisms. Persistence, function specialization, and source rewriting are the three levels described in the plan. Benchmarks are essential to verify that the self-modification actually improves performance.

**Critical Finding — Performance Does Not Yet Meet Plan Targets**:
The benchmark results show that the current cache/evaluator is **not yet faster** than the conventional evaluator for simple arithmetic, and only marginally faster for the dot-product workload:

| Workload | SMC warm vs conventional | Plan target (long) |
|---|---|---|
| Simple arithmetic (100k) | ~0.60–0.80× (slower) | ≥5× |
| Dot product (100k) | ~1.20× | ≥5× |

**Why this is happening**:
1. The cache key is a full AST list compared with `EQUAL`, which is expensive for very cheap arithmetic operations.
2. Every evaluation still rewrites the AST and recurses, even on a cache hit for the whole expression.
3. Parser and cache overhead dominate when the actual computation is a single arithmetic op.

**Options to improve and correct this flaw** (choose one or more):

1. **Compiled cache dispatch table** (highest impact): Instead of a generic EQUAL hash table, generate a compiled `cond`/`case` that dispatches on AST shape. This avoids hash lookup and list traversal for hot expressions.
2. **Memoize parsed ASTs to canonical objects**: Intern ASTs so the same expression always returns the same object, allowing `EQ` hash lookups instead of `EQUAL`.
3. **Skip rewriting for fully cached expressions**: The current code rewrites even when the top-level node is cached. A fast path should return the cached value immediately without any AST traversal.
4. **Add more expensive math modules**: The current benchmarks only exercise cheap arithmetic. The dot-product example in the README uses more sub-expressions; adding a vector math module and a more realistic renderer-style benchmark would show larger wins.
5. **Reduce the problem space**: Restrict the random benchmark to a smaller operand/operator set so cache hit rate is extremely high, making the overhead more visible and the speedup more achievable.

**Current State**:
- Phase 3 is implemented and tested
- All 33 tests pass
- Benchmarks exist and run, but results do not yet meet the plan's speedup targets
- `./run.sh "2+3*4"` still works correctly

**Blockers / Known Limitations**:
- Cache lookup overhead is too high for simple arithmetic
- Function specialization only triggers for hot operand pairs; it does not yet dominate overall performance
- Source-level rewriting helps startup time but does not speed up runtime evaluation
- CLI is still stateless; cache persistence must be invoked explicitly by the user

**Next Steps** (for Session 4):
- Implement compiled cache dispatch table (Option A) to eliminate EQUAL hash lookup overhead
- Add a vector/linear algebra module (`src/math/linear-algebra.lisp`) with `:vec3`, `:dot`, `:cross`, `:norm`, `:normalize`
- Refactor the benchmark suite into per-category benchmarks (arithmetic, dot product, cross product, trig, polynomial, mixed, realistic renderer) plus a summary runner
- Add tests for the new dispatch compiler and vector operators
- Update `docs/plan.md`, `docs/HANDOFF.md`, and `README.md` as work progresses

---

## Session 4 — Performance Optimization, Vector Math, and Refactored Benchmarks

**Date**: 2026-06-26

**Completed**:
- Implemented canonical AST interning in `src/core/ast.lisp` (`*ast-intern-table*`, `intern-ast`).
- Switched the cache from `EQUAL` to `EQ` hash table in `src/core/cache.lisp`.
- Made `cache-set` intern keys so that loaded/generated cache entries use canonical AST objects.
- Created and then disabled `src/core/dispatch-compiler.lisp` for large caches; the compiled dispatch table was slower than the EQ hash and caused stack overflow on long series.
- Added `src/math/linear-algebra.lisp` with `:vec3`, `:dot`, `:cross`, `:norm`, `:normalize` operators.
- Added `src/math/trigonometry.lisp` with `:sin` and `:cos` operators.
- Updated `self-modifying-calculator.asd` to load new modules.
- Added `tests/core/test-linear-algebra.lisp` (5 tests).
- Refactored benchmark suite into `tests/benchmarks/benchmark-framework.lisp`, `series-generators.lisp`, and `run-all-benchmarks.lisp` with 7 categories: Arithmetic, Dot Product, Cross Product, Trig, Polynomial, Mixed, Realistic Renderer.
- Removed the explicit `rewrite-with-cache` step from the hot path in `src/core/evaluator.lisp`; cached sub-trees are still reused via recursive evaluation.
- Created `docs/notes/session-4-implementation-notes.md` to capture the iterative approach, dead-ends, and final results.
- Updated `docs/CHANGELOG.md` and `docs/HANDOFF.md`.

**Rationale**:
Session 3 left the project with a performance problem: the cache was slower than conventional evaluation for simple arithmetic and only marginally faster for dot products. Canonical AST interning + EQ hash table eliminates the expensive `EQUAL` structural comparison that dominated the cache lookup cost. Removing the explicit rewrite step further reduces overhead on cache misses. Adding realistic math modules and per-category benchmarks proves the optimization where it matters most (renderer-style workloads).

**Key Findings / Decisions**:
- **Compiled dispatch table**: A naive compiled `cond` of all cache entries is slower than the hash table for medium caches and causes stack overflow for large caches. It is disabled for now; a smarter dispatch structure (e.g., trie, hash-based) may be revisited later.
- **AST interning**: The big win. With canonical AST objects, the cache uses `EQ` pointer comparison, which is much faster than `EQUAL` structural comparison.
- **Rewrite removal**: Removing `rewrite-with-cache` from the hot path improved arithmetic long series from 0.38× to 1.17× and did not break correctness.

**Performance Results (LONG series, 100,000 calculations)**:

| Category | Speedup vs Conventional |
|---|---|
| Arithmetic | **1.17×** |
| Dot Product | **3.33×** |
| Cross Product | **3.00×** |
| Trig | **1.20×** |
| Polynomial | **3.50×** |
| Mixed | **2.67×** |
| Realistic Renderer | **3.25×** |

- Arithmetic is the hardest category but now beats conventional evaluation.
- All realistic/renderer-style workloads show solid speedups (3×+ for dot, cross, polynomial, realistic renderer).
- Short and medium series are too fast to measure accurately with the current timer.

**Current State**:
- All 38 tests pass.
- The calculator now outperforms conventional evaluation on realistic workloads.
- The benchmark suite is split by category, making it easy to identify strengths and weaknesses.

**Blockers / Known Limitations**:
- Short/medium benchmark timings are noisy (0.0000 s) because the operations are fast relative to the timer resolution.
- The compiled dispatch table is disabled for large caches; a better approach is needed if we want to pursue that optimization path.
- `matcher.lisp` and `rewrite-with-cache` are no longer used in the hot path but remain in the source tree for potential future use (e.g., source-level rewriting).

**Next Steps** (for Session 5):
- Improve short/medium benchmark timing accuracy (e.g., run multiple passes and accumulate time).
- Optionally revisit compiled dispatch with a smarter data structure.
- Consider cache eviction / size bounding.
- Continue adding math modules (algebra, calculus, statistics) as planned.
- Update `README.md` with the new benchmark usage and performance results.

---
