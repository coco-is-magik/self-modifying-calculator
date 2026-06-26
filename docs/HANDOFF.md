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
- Implemented `src/core/evaluator.lisp` with operator registry, variable environment, and recursive AST evaluation
- Implemented `src/interface/parser.lisp` with tokenizer and recursive-descent parser supporting `+ - * / ^`, precedence, parentheses, and floats
- Implemented `src/math/arithmetic.lisp` as the first pluggable math module
- Implemented `src/main.lisp` as the entry point and `run-calculator` helper
- Created `self-modifying-calculator.asd` ASDF system definition
- Created `run.sh` convenience wrapper for CLI usage
- Created a lightweight, dependency-free test runner in `tests/test-runner.lisp`
- Added core tests: `tests/core/test-ast.lisp`, `tests/core/test-parser.lisp`, `tests/core/test-evaluator.lisp`
- Updated `README.md` with SBCL requirement, new usage examples, and test commands
- Updated `docs/CHANGELOG.md` with Session 1 entries

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
