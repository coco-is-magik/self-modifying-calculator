# Changelog

> **Purpose**: This document records every meaningful change to the project, the rationale behind it, and the impact it has on the project. It is ordered from newest to oldest.
>
> **How to use**: After making a change (file edits, new features, refactors, bug fixes, plan updates), append a new entry at the top with: date, change summary, affected files, rationale, and any notes for the future.

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
