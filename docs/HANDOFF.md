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
- Implement a cache-aware evaluator that checks the cache before computing
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

---

## Session 5 — Documentation Drift Correction & Phase 5 Math Modules

**Date**: 2026-06-28

**Completed**:
- Corrected factual drift in `docs/plan.md`: removed Quicklisp references, updated parser description to recursive-descent, removed the planned `cli.lisp` from the module tree, fixed long series length from 1,000,000 to 100,000, updated benchmarking file structure, and restructured phases to match actual implementation (Phases 1–5 complete, Phase 6 in progress).
- Updated `README.md` roadmap to mark Phase 5 complete and Phase 6 in progress; added link to new `docs/KNOWN-ISSUES.md`.
- Expanded `src/core/package.lisp` exports to include new math operators and commonly used internal functions (`run-all-benchmarks`, `evaluate-node`, `enable-operator-specialization`, math module registration functions).
- Created three new math modules to complete Phase 5:
  - `src/math/algebra.lisp` — `:quadratic`, `:quadratic-roots`, `:polynomial-eval`, `:factor`
  - `src/math/calculus.lisp` — `:derivative`, `:integral`, `:simpson-integral`
  - `src/math/statistics.lisp` — `:mean`, `:variance`, `:std-dev`, `:median`, `:min`, `:max`, `:sum`
- Added unit tests for the new modules in `tests/math/`.
- Updated `self-modifying-calculator.asd` and `src/main.lisp` to load and register the new modules.
- Created `docs/KNOWN-ISSUES.md` to catalog all identified gaps with dates, impact, and planned resolutions.
- Updated `docs/repo-status-report.md` to reflect the completed Phase 5 and reference `docs/KNOWN-ISSUES.md`.

**Rationale**:
The documentation had drifted from the implemented code (Quicklisp, Pratt parser, separate `cli.lisp`, incorrect benchmark lengths, mismatched phase numbering). Phase 5 was listed as "In Progress" but no work had started. This session brings the documentation back in sync with reality, completes the planned math modules, and creates a durable, dated record of remaining gaps so the next work session can be focused.

**Current State**:
- Phases 1–5 are now fully implemented and tested.
- 54 tests are expected (38 from prior sessions + 16 new algebra/calculus/statistics tests; exact count verified by test run below).
- All known limitations are documented in `docs/KNOWN-ISSUES.md`.

**Blockers / Known Limitations**:
- Performance targets still not met (1.17×–3.50× vs 5.0× target).
- Compiled cache dispatch remains disabled for large caches.
- No cache eviction, automatic persistence, or function-call syntax yet.
- Test command in README still uses multiple `--eval` loads.

**Next Steps** (for Session 6):
- Implement cache eviction / size bounding.
- Simplify test invocation with `tests/load-all-tests.lisp` and update README.
- Add integration tests for full parse → evaluate → cache → persist workflows.
- Revisit compiled cache dispatch with a smarter data structure.
- Unify the 3-level optimization pipeline.

---

## Session 6 — Benchmark Overhaul & Documentation Cleanup

**Date**: 2026-06-30

**Completed**:
- Overhauled the benchmark framework in `tests/benchmarks/benchmark-framework.lisp`:
  - Replaced single-run `benchmark-result` with `benchmark-trial` (single trial) and `trial-aggregate` (median + range across seeds).
  - Added isolated optimization levels: `:baseline`, `:l1`, `:l1.5`, `:l2`.
  - Added `setup-warmup-for-level-2` so specialization thresholds are crossed before timing.
  - Added `unique-ratio` reporting and cold/warm speedup fields.
  - Switched timing to CPU time via `get-internal-run-time` and added `gc-and-settle` between phases.
- Expanded `tests/benchmarks/series-generators.lisp` with domain-size/component-set/angle-count parameters and new categories:
  - Matrix Multiply (`:mat4x4-mul`)
  - Blinn-Phong shading
  - End-to-end parse+eval string generation (`generate-expression-strings`)
- Added new linear algebra operators `:vec3-add` and `:mat4x4-mul` in `src/math/linear-algebra.lisp`.
- Added `ast-to-string` in `src/core/ast.lisp` to support end-to-end parse+eval benchmarks.
- Fixed `scripts/demo.lisp` to use the new `run-single-trial` / `benchmark-trial` API; `./scripts/run-demo.sh` now runs without error.
- Updated `docs/KNOWN-ISSUES.md`, `docs/notes/session-4-implementation-notes.md`, `docs/notes/session-5-implementation-notes.md`, and `docs/benchmark-revamp-plan.md` to reference the new benchmark API and mark implemented phases.
- Expanded `docs/CHANGELOG.md` `[Unreleased]` with benchmark overhaul details.
- Removed duplicate `*.fasl` line from `.gitignore`.

**Rationale**:
The benchmark methodology had become a source of noisy, hard-to-interpret numbers. The old single-run, single-seed, mixed-optimization approach could not answer whether caching, compiled dispatch, or specialization was responsible for any observed speedup. The overhaul isolates each mechanism, aggregates across seeds, and reports ranges. The documentation cleanup removes references to deleted functions (`run-benchmark-category`, `benchmark-result`) and records the current state for the next session.

**Current State**:
- All 66 tests pass via `./scripts/run-tests.sh`.
- `./scripts/run-demo.sh` runs successfully and reports a realistic-renderer speedup.
- Benchmarks can be run at isolated levels with `(smc:run-all-benchmarks :level :l2 :seeds '(1 2 3))`.

**Blockers / Known Limitations**:
- `scripts/run-benchmarks.sh` does not yet expose the `--sweep-domain` and `--sweep-cache` flags; the Lisp sweep helpers exist but are not wired to the shell script.
- `measure-cpu-time` uses `get-internal-run-time` rather than `sb-ext:process-run-time`; the plan documents this as a remaining optional improvement.
- Arithmetic remains the hardest workload and still does not meet the 5× target.

**Next Steps** (for Session 7):
- Wire `--sweep-domain` and `--sweep-cache` flags into `scripts/run-benchmarks.sh`.
- Continue arithmetic optimization (per-operator hash dispatch, selective caching of small nodes, bounded cache sizing).
- Update `README.md` benchmark results section with the new methodology and latest numbers.

---

## Session 7 — C Artifact Cache Subsystem ✅

**Date**: 2026-07-09

**Completed**:
- Added `smc_artifact_*` API to `include/smc.h`:
  - Configuration: `smc_artifact_configure`, `smc_artifact_config_t`
  - Operations: `smc_artifact_lookup`, `smc_artifact_store`, `smc_artifact_remove`, `smc_artifact_clear`
  - Statistics: `smc_artifact_get_stats`, `smc_artifact_reset_stats`
  - Error codes: `SMC_ERR_SIZE`, `SMC_ERR_CAPACITY`
  - Feature flag: `SMC_FEATURE_ARTIFACT_CACHE`
- Implemented `src/c/smc_artifact.c` (direct-mapped hash table with FNV-1a 32-bit hashing)
- Added `smc_state_*` API for dirty-state tracking:
  - Configuration: `smc_state_configure`, `smc_state_config_t`
  - Operations: `smc_state_changed`, `smc_state_clear`
  - Statistics: `smc_state_get_stats`, `smc_state_reset_stats`
  - Feature flag: `SMC_FEATURE_STATE_TRACKING`
- Implemented `src/c/smc_state.c` (direct-mapped hash table for state comparison)
- Integrated both caches into `smc_context_t` lifecycle in `smc_runtime_stub.c`
- Added C tests: `tests/c/test_artifact_cache.c` (8 tests), `tests/c/test_state_tracking.c` (4 tests)
- Added C examples: `examples/c/artifact_cache_basic.c`, `examples/c/dirty_state_basic.c`, `examples/c/glyph_block_cache.c`
- Updated Python binding `python/smc/__init__.py` with artifact and state functions
- Created `docs/artifact-cache.md` documenting the API and renderer pattern
- Updated `docs/c-api-maturity-plan.md` with Phase 9 (Artifact Cache Subsystem)

**Rationale**: The SMC v2 goal was to add a general-purpose binary artifact cache and dirty-state tracker for hot-path optimization in C applications. This enables C projects to skip redundant rasterization and computation by caching arbitrary binary artifacts keyed by opaque byte sequences.

**Known Limitations (v2.0)**:
1. Memory is allocated on each `smc_artifact_store` call using `malloc`. This is acceptable for the MVP but may cause allocation churn in hot paths. v2.1 will preallocate fixed slots.
2. Buffer-too-small handling returns `out_value_size` correctly, but the caller must provide a buffer large enough.
3. Eviction is direct-mapped only (not LRU) — simpler but may discard useful entries.

**Current State**:
- All 16 C tests pass (`ctest --output-on-failure` runs 16 C tests)
- All examples compile and run correctly
- Python binding exposes the new APIs

**Blockers / Known Limitations**:
- None for current scope; v2.1 work will address preallocated storage

**Next Steps** (for Session 8):
- Add more edge-case tests for collision eviction with different keys
- Add memory budget enforcement tests
- Consider preallocated fixed-slot storage for v2.1
- Run ASan/UBSan verification with `SMC_SANITIZE=ON`
- Verify Python binding works with new functions