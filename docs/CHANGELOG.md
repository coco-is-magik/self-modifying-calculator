# Changelog

> **Purpose**: This document records every meaningful change to the project, the rationale behind it, and the impact it has on the project. It is ordered from newest to oldest.
>
> **How to use**: After making a change (file edits, new features, refactors, bug fixes, plan updates), append a new entry at the top with: date, change summary, affected files, rationale, and any notes for the future.


## 2026-06-28 — Session 5: Documentation Drift Correction and Phase 5 Math Modules

**Change**: Fixed factual drift in `docs/plan.md` and `README.md`, completed the three missing Phase 5 math modules (algebra, calculus, statistics), and created a dated `docs/KNOWN-ISSUES.md` tracking all remaining gaps.

**Affected Files**:
- `docs/plan.md` (updated to reflect implemented state: no Quicklisp, recursive-descent parser, 100,000-calculation long series, corrected phase structure)
- `README.md` (updated roadmap, added Phase 6 in-progress items, linked `docs/KNOWN-ISSUES.md`)
- `src/core/package.lisp` (expanded exports)
- `src/main.lisp` (registered new math modules)
- `self-modifying-calculator.asd` (added new math modules)
- `src/math/algebra.lisp` (created)
- `src/math/calculus.lisp` (created)
- `src/math/statistics.lisp` (created)
- `tests/math/test-algebra.lisp` (created)
- `tests/math/test-calculus.lisp` (created)
- `tests/math/test-statistics.lisp` (created)
- `docs/KNOWN-ISSUES.md` (created)
- `docs/repo-status-report.md` (updated status)
- `docs/HANDOFF.md` (updated with Session 5 entry)
- `docs/CHANGELOG.md` (this file)

**Rationale**: The documentation had drifted from the implemented code, and Phase 5 was marked "In Progress" without any code started. This session reconciles docs with reality and completes the planned math module coverage so the project can move into Phase 6 polish work.

**Details**:
- New algebra operators: `:quadratic`, `:quadratic-roots`, `:polynomial-eval`, `:factor`.
- New calculus operators: `:derivative`, `:integral`, `:simpson-integral` (numerical, operating on polynomial coefficient lists).
- New statistics operators: `:mean`, `:variance`, `:std-dev`, `:median`, `:min`, `:max`, `:sum`.
- Each module follows the existing registration pattern: helper functions + `register-*-operators` + auto-registration on load.
- Expanded `src/core/package.lisp` exports to include new operators and internal utilities previously accessed via `smc::`.

**Notes for Future**: Phase 6 items remain open. See `docs/KNOWN-ISSUES.md` for the complete, dated list.

---

## 2026-06-29 — Session 5: Cache Eviction, Benchmark Timing, and Test Runner

**Change**: Added LRU cache eviction, improved short/medium benchmark timing accuracy, fixed an algebra compiler warning, and created a single test runner script.

**Affected Files**:
- `src/core/cache.lisp` (updated with `max-size`, `access-counter`, `last-access`, LRU eviction)
- `tests/core/test-cache.lisp` (added LRU eviction test)
- `tests/benchmarks/benchmark-framework.lisp` (added repeated passes for short/medium series)
- `src/math/algebra.lisp` (fixed `quadratic-roots` variable warning)
- `tests/load-all-tests.lisp` (created)
- `tests/integration/test-integration.lisp` (created)
- `README.md` (updated test command, roadmap, and performance notes)
- `docs/KNOWN-ISSUES.md` (marked cache eviction and README test command as resolved)
- `docs/CHANGELOG.md` (this file)

**Rationale**: Cache eviction prevents unbounded memory growth in long-running sessions. The test runner simplifies verification. The benchmark timing fix makes short/medium measurements more reliable.

**Details**:
- `make-cache` now accepts an optional `max-size` argument; when full, `cache-set` evicts the least recently used entry.
- `cache-get` and `cache-set` update the access counter to maintain LRU order.
- Short benchmarks run 100× and medium benchmarks run 10× to accumulate measurable time.
- Fixed the `|2A|` undefined variable warning in `quadratic-roots` by binding `2a` in the outer `let`.
- Added `tests/load-all-tests.lisp` as the single command to run all 55 tests.

**Verification**: All 55 tests pass.

---

## 2026-06-26 — Session 4: Performance Optimization, Vector Math, and Refactored Benchmarks


**Change**: Implemented canonical AST interning + EQ hash table, added vector and trigonometry modules, refactored the benchmark suite into per-category benchmarks, and removed the explicit `rewrite-with-cache` overhead from the hot path.

**Affected Files**:
- `src/core/ast.lisp` (updated with `*ast-intern-table*` and `intern-ast`)
- `src/core/cache.lisp` (updated to use EQ hash table and intern keys on `cache-set`)
- `src/core/evaluator.lisp` (removed explicit `rewrite-with-cache` from hot path; updated `make-ast` to wrap numeric literals)
- `src/core/dispatch-compiler.lisp` (created and then disabled for large caches)
- `src/math/linear-algebra.lisp` (created)
- `src/math/trigonometry.lisp` (created)
- `self-modifying-calculator.asd` (updated to include new files)
- `tests/core/test-linear-algebra.lisp` (created)
- `tests/benchmarks/benchmark-framework.lisp` (created)
- `tests/benchmarks/series-generators.lisp` (created)
- `tests/benchmarks/run-all-benchmarks.lisp` (created)
- `docs/notes/session-4-implementation-notes.md` (created)
- `docs/HANDOFF.md` (updated with Session 4 entry)
- `docs/CHANGELOG.md` (this file)

**Rationale**: Session 3 identified that the cache-aware evaluator was slower than conventional evaluation for simple arithmetic and only marginally faster for dot products. The root cause was the `EQUAL` hash lookup on AST lists being more expensive than the operations it skipped. Session 4 corrects this with canonical AST interning and an EQ hash table.

**Details**:
- Added global AST intern table so identical expressions produce the same object, enabling `EQ` hash lookups.
- Switched cache from `EQUAL` to `EQ` for faster lookup.
- Created `src/core/dispatch-compiler.lisp` as an experimental compiled dispatch table; it was disabled for large caches because the linear scan of thousands of clauses was slower than the hash table and caused stack overflow on long series.
- Added linear algebra operators `:vec3`, `:dot`, `:cross`, `:norm`, `:normalize`.
- Added trigonometry operators `:sin` and `:cos`.
- Refactored benchmarks into 7 categories: Arithmetic, Dot Product, Cross Product, Trig, Polynomial, Mixed, and Realistic Renderer.
- Removed explicit `rewrite-with-cache` from the hot path; cached sub-trees are still reused because children are evaluated through `evaluate-node-cached`.
- All 38 tests pass (33 from previous sessions + 5 new linear algebra tests).

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

- Arithmetic is the hardest category (operations are very cheap), but it is now faster than conventional.
- All realistic/renderer-style workloads show solid speedups.
- Short and medium series are too fast to measure accurately with the current timer; see `docs/notes/session-4-implementation-notes.md` for future improvements.

**Notes for Future**:
- Improve short/medium benchmark timing accuracy (e.g., run multiple passes and accumulate time).
- Revisit compiled dispatch with a smarter data structure (e.g., hash-based dispatch) that avoids the linear-scan/stack-overflow problem.
- Consider cache eviction / size bounding.
- Continue adding math modules (algebra, calculus, statistics) as planned.

---

## 2026-06-26 — Session 3: Phase 3 Self-Modification and Benchmarking

**Change**: Implemented all three levels of self-modification (cache persistence, runtime function specialization, source-level rewriting) and added benchmarking harnesses.

**Affected Files**:
- `src/core/cache.lisp` (updated with `save-cache` and `load-cache`)
- `src/core/optimizer.lisp` (created)
- `src/core/self-writer.lisp` (created)
- `src/core/evaluator.lisp` (updated to store original operator functions)
- `src/interface/parser.lisp` (updated with `*parse-cache*`)
- `self-modifying-calculator.asd` (updated to include new files)
- `tests/core/test-optimizer.lisp` (created)
- `tests/core/test-self-writer.lisp` (created)
- `tests/benchmarks/arithmetic-benchmark.lisp` (created)
- `tests/benchmarks/dot-product-benchmark.lisp` (created)
- `docs/HANDOFF.md` (updated with Session 3 entry)
- `docs/CHANGELOG.md` (this file)

**Rationale**: The project name is "Self-Modifying Calculator"; this session delivers the actual self-modification mechanisms described in the plan. The benchmarks are required to prove that the self-modification improves performance.

**Details**:
- Added Level 1 persistence: `save-cache` and `load-cache` for `cache/cache.sexp`
- Added Level 2 specialization: `optimizer.lisp` tracks hot operand patterns, generates compiled short-circuit functions, and hot-swaps them
- Added Level 3 source rewriting: `self-writer.lisp` emits a loadable Lisp file with `cache-set` literals
- Added parser cache to avoid re-tokenizing/re-parsing repeated strings
- Added arithmetic and dot-product benchmarks

**Important Finding**: The current implementation is not yet faster than conventional evaluation for simple arithmetic and only ~1.20× faster for the dot-product benchmark. The plan's targets (5× long series, 20× stretch) are not met. See `docs/HANDOFF.md` Session 3 for a full analysis and five proposed corrective options.

**Notes for Future**: The next session should focus on reducing cache lookup overhead. Top candidates are compiled cache dispatch tables and canonical AST interning.

---

## 2026-06-26 — Session 2: Phase 2 Caching Engine Implemented

**Change**: Implemented the hierarchical expression cache, matcher, and cache-aware evaluator.

**Affected Files**:
- `src/core/cache.lisp` (created)
- `src/core/matcher.lisp` (created)
- `src/core/evaluator.lisp` (updated with cache-aware evaluation)
- `self-modifying-calculator.asd` (updated to include new files)
- `tests/core/test-cache.lisp` (created)
- `docs/HANDOFF.md` (updated with Session 2 entry)
- `docs/CHANGELOG.md` (this file)

**Rationale**: Caching is the core optimization mechanism for the self-modifying calculator. Hierarchical caching allows the system to reuse both whole expressions and their sub-expressions, directly supporting the renderer and quadratic use cases described in the plan.

**Details**:
- Added `cache` struct with EQUAL hash table, hit/miss counters, and statistics
- Added `rewrite-with-cache` to replace cached sub-trees with constant nodes
- Integrated cache into the evaluator via `evaluate-node-cached` and `evaluate-node-uncached`
- Prevented caching of expressions containing variables (avoiding incorrect reuse across different variable bindings)
- Added 5 cache tests covering cache operations, variable exclusion, repeated evaluation, sub-expression reuse, and statistics

**Notes for Future**: Phase 3 should add cache persistence across sessions and runtime function specialization. The current cache is in-memory only and resets when the SBCL process exits.

---

## 2026-06-26 — Session 1: Phase 1 Foundation Implemented

**Change**: Implemented the foundational SBCL/ASDF project structure, AST representation, parser, evaluator, arithmetic module, CLI wrapper, and a dependency-free test suite.

**Affected Files**:
- `src/core/package.lisp` (created)
- `src/core/ast.lisp` (created)
- `src/core/evaluator.lisp` (created)
- `src/interface/parser.lisp` (created)
- `src/math/arithmetic.lisp` (created)
- `src/main.lisp` (created)
- `self-modifying-calculator.asd` (created)
- `run.sh` (created)
- `tests/test-runner.lisp` (created)
- `tests/core/test-ast.lisp` (created)
- `tests/core/test-parser.lisp` (created)
- `tests/core/test-evaluator.lisp` (created)
- `README.md` (updated with SBCL requirement and usage)
- `docs/HANDOFF.md` (updated with Session 1 entry)

**Rationale**: Phase 1 provides the foundation for all subsequent caching and self-modification work. A working AST, parser, and evaluator are prerequisites for the cache-aware evaluation that defines the project.

**Details**:
- Added `:self-modifying-calculator` package with nickname `smc`
- Implemented constant, variable, and operator AST nodes with traversal utilities
- Built a recursive-descent parser with precedence, parentheses, and floats
- Implemented operator registry and variable environment in the evaluator
- Created the first pluggable math module (`arithmetic.lisp`) registering `+ - * / ^`
- Wrote a dependency-free test runner and 24 passing tests covering AST, parser, and evaluator
- Added `./run.sh` convenience wrapper for CLI use

**Bug Fixes**:
- `src/core/evaluator.lisp`: `evaluate-node` now recursively evaluates arguments before applying operator functions (was passing raw AST nodes)
- `src/interface/parser.lisp`: rewrote parser around a mutable `token-stream` struct because parser functions were mutating local parameter copies and failing to consume tokens

**Notes for Future**: Phase 2 can now begin. The cache-aware evaluator should replace the current `evaluate-node` in `src/core/evaluator.lisp` or wrap it, using the cache to short-circuit whole and sub-expressions.

---

## 2026-06-26 — Session 0: Project Planning Complete

**Change**: Finalized the project architecture and created initial planning documents.

**Affected Files**:
- `docs/plan.md` (created)
- `docs/HANDOFF.md` (created)
- `README.md` (updated)

**Rationale**: Before writing implementation code, the project needed a shared source of truth describing the goals, architecture, and roadmap. This avoids drift and gives future engineers a clear reference.

**Details**:
- Defined hierarchical expression caching (whole + sub-expression caching)
- Specified three levels of self-modification
- Added modular math module structure
- Added performance benchmarking suite with explicit speedup targets
- Updated README to articulate the new project vision and roadmap

**Notes for Future**: Implementation begins in Session 1. The first implementation decision is whether SBCL is available; if not, we need to install it or fall back to a portable Common Lisp solution.

---
