# Changelog

> **Purpose**: This document records every meaningful change to the project, the rationale behind it, and the impact it has on the project. It is ordered from newest to oldest.
>
> **How to use**: After making a change (file edits, new features, refactors, bug fixes, plan updates), append a new entry at the top with: date, change summary, affected files, rationale, and any notes for the future.

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
