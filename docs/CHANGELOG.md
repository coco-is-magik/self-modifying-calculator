# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

### Added
- C API maturity plan in `docs/c-api-maturity-plan.md`, organized into eight phases to be implemented one at a time.
- Formal ABI contract in `docs/abi-contract.md` covering versioning, struct layout, ownership, null-pointer behavior, pre-init rules, thread safety, and error codes.
- Header `include/smc.h` rewritten with explicit ABI contract comments, `@tooling`/`@production` tier labels, and new error codes (`SMC_ERR_ABI`, `SMC_ERR_ARITY`, `SMC_ERR_NOT_FOUND`, `SMC_ERR_THREAD`, `SMC_ERR_SHUTDOWN`).
- Introspection functions `smc_abi_version()` and `smc_runtime_kind()`.
- Python bindings for `smc.abi_version()` and `smc.runtime_kind()`.
- Documentation pointers updated in `README.md`, `docs/embedding-roadmap.md`, `docs/integration-guide.md`, and `docs/KNOWN-ISSUES.md` to reference the new C API maturity plan and ABI contract.
- Function call syntax in the string parser: `sin(x)`, `dot(a,b)`, `vec3(x,y,z)`, etc.
- Parser cache eviction with configurable `*parse-cache-max-size*` (default 1000).
- Automatic cache persistence: `*auto-persist-cache*` loads the cache on startup and saves it on exit.
- Unified 3-level optimization pipeline via `configure-optimization` in `src/core/pipeline.lisp`.
- Cache-set hook (`*cache-set-hook*`) for automatic Level 2 specialization and Level 3 source rewriting.
- Compiled dispatch size limit (`*compiled-dispatch-max-clauses*`) to avoid stack overflow on large caches.
- `.gitignore` for generated cache files and compiled Lisp files.
- Performance profiling notes in `docs/notes/performance-profiling.md`.
- Benchmark overhaul: multi-trial, multi-seed aggregation with `benchmark-trial` / `trial-aggregate`.
- Isolated optimization levels in benchmarks: `:baseline`, `:l1`, `:l1.5`, `:l2`.
- New benchmark categories: Matrix Multiply, Blinn-Phong shading, and end-to-end parse+eval.
- Domain-size and cache-size sweep helpers in `tests/benchmarks/run-all-benchmarks.lisp`.
- `ast-to-string` in `src/core/ast.lisp` for end-to-end parse+eval benchmarking.
- New linear algebra operators: `:vec3-add` and `:mat4x4-mul`.

### Changed
- Moved generated cache source output from `src/generated/` to `cache/generated/`.
- Re-enabled compiled dispatch in the evaluator for small caches; large caches fall back to the EQ hash table.
- Updated `src/core/package.lisp` to export parser, cache persistence, and pipeline symbols.
- Updated documentation (`README.md`, `docs/KNOWN-ISSUES.md`, `docs/repo-status-report.md`).
- `run-all-benchmarks` now enables Level 2 operator specialization by default.
- Moved `run.sh` and `demo.lisp` into a new `scripts/` folder.
- Added `scripts/run-calculator.sh`, `scripts/run-tests.sh`, `scripts/run-benchmarks.sh`, `scripts/run-demo.sh`, `scripts/run-linter.sh`, and `scripts/run.sh` (backward-compatible alias).
- Added `scripts/README.md` documenting every script.

### Fixed
- Compiled dispatch now correctly quotes cached list values to avoid "illegal function call" errors.
- `generate-specialized-function` now emits a `&rest` lambda so variadic operators like `+` are handled correctly.
- `*cache-set-hook*` is now declared before `cache-set` to avoid compilation warnings.

### Removed
- Deleted `src/core/matcher.lisp` (dead code; the evaluator caches sub-expressions directly without rewriting).
- Removed `src/core/matcher.lisp` from `self-modifying-calculator.asd`.
- Deleted `tests/benchmarks/arithmetic-benchmark.lisp` and `tests/benchmarks/dot-product-benchmark.lisp` (superseded by the unified benchmark framework).

### Performance
- Avoided creating a fresh `*variable-table*` hash table in `evaluate` when no variables are bound.
- Made LRU bookkeeping conditional on `max-size > 0` for unlimited caches.
- Added fast paths for constants and variables in `evaluate-node-cached`, bypassing cache lookup.
- Realistic-renderer LONG benchmark now consistently exceeds the 5× target (range 5×–8×).

## [0.1.0] - 2026-06-28

### Added
- Initial SBCL/ASDF project structure.
- AST representation with canonical interning.
- Recursive-descent string parser.
- Cache-aware evaluator with EQ hash table and hierarchical caching.
- Math modules: arithmetic, algebra, calculus, linear algebra, trigonometry, statistics.
- Benchmark suite with 7 categories.
- Demo script (`demo.lisp`).
- Integration tests and unit tests.
