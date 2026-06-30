# Known Issues & Technical Debt

> **Date**: 2026-06-28
> **Purpose**: Track all known gaps, limitations, and technical debt items. Each entry includes the date discovered, a description, impact, and any planned or completed resolution.

---

## Performance

1. **Performance targets partially met** (2026-06-28, updated 2026-06-29)
   - **Description**: Plan targets 5.0× speedup for long series and 20× stretch. After tuning, the priority category (realistic renderer) now reaches 5.0×–8.0×. Dot product, cross product, and polynomial are often above 5.0× but very noisy (2.3×–8.3×). Trig is usually 2.3×–5.4×. Mixed is 2.0×–5.2×. Arithmetic remains the hardest case at ~1.0×–1.9×.
   - **Impact**: The calculator meets the 5× minimum target on the most important realistic workload. Arithmetic is still slower than conventional evaluation because the operations are too cheap to amortize cache overhead.
   - **Resolution**: Implemented variable-table allocation avoidance, conditional LRU, fast paths for constants/variables, and Level 2 specialization in benchmark runs. See `docs/notes/performance-profiling.md`.
   - **Planned resolution**: Further options include per-operator hash dispatch, selective caching of small arithmetic nodes, bounded cache sizing, and inlining operator-function lookups.

2. **Compiled cache dispatch limited to small caches** (2026-06-28)
   - **Description**: `src/core/dispatch-compiler.lisp` now limits compiled dispatch to caches with at most `*compiled-dispatch-max-clauses*` entries (default 100). Larger caches fall back to the EQ hash table, avoiding the stack overflow and slow linear scan caused by thousands of cond clauses. Cached list values are quoted in the generated code so they are returned as literals rather than function calls.
   - **Impact**: The compiled dispatch path is now safe to enable by default for small caches; large caches continue to use the fast EQ hash table.
   - **Resolution**: Added `*compiled-dispatch-max-clauses*`; re-enabled compiled dispatch in `evaluate-node-cached`; disabled recompilation when the cache grows beyond the limit. See `src/core/dispatch-compiler.lisp` and `src/core/evaluator.lisp`.

3. **Short/medium benchmark timing noise** (2026-06-28)
   - **Description**: Short (100) and medium (10,000) calculation series are now measured by repeating each pass 100× and 10× respectively, then dividing by the repetition count. Timings are now measurable, though long series remain the most reliable source of speedup numbers.
   - **Impact**: Short/medium speedup numbers are now mostly reliable, with occasional run-to-run variance.
   - **Resolution**: `*benchmark-repeats*` in `tests/benchmarks/benchmark-framework.lisp` controls the repetition count, and `run-single-trial` accumulates repetitions for short and medium series. Tuning may still be needed if variance remains high.

---

## Cache & Memory

4. **Cache eviction policy implemented** (2026-06-28)
   - **Description**: `*global-cache*` and per-cache instances can be created with a positive `max-size`. When the cache is full, the least recently used entry is evicted on the next `cache-set` of a new key.
   - **Impact**: Long-running sessions can now bound memory usage.
   - **Resolution**: `make-cache` accepts an optional `max-size` argument; `cache-touch`, `cache-evict-lru`, and updated `cache-set`/`cache-get` implement LRU semantics. Test added in `tests/core/test-cache.lisp`.

5. **Automatic cache persistence implemented** (2026-06-28)
   - **Description**: `save-cache` and `load-cache` can now be invoked automatically via `maybe-save-global-cache` and `maybe-load-global-cache` when `*auto-persist-cache*` is true. The `main` CLI entry point loads the cache on startup and saves it on successful exit when auto-persistence is enabled.
   - **Impact**: Sessions can now retain a warm cache across restarts by setting `*auto-persist-cache*` to `t`.
   - **Resolution**: Added `*auto-persist-cache*` flag, `maybe-load-global-cache`, and `maybe-save-global-cache` in `src/core/cache.lisp`; updated `src/main.lisp` to load/save around CLI execution. Integration test added in `tests/integration/test-integration.lisp`.

6. **Parser cache eviction implemented** (2026-06-28)
   - **Description**: `*parse-cache*` now has a configurable maximum size. When the number of entries reaches `*parse-cache-max-size*`, the cache is cleared before the next insertion.
   - **Impact**: Long-running sessions with many distinct input strings no longer grow the parse cache without bound.
   - **Resolution**: `*parse-cache-max-size*` defaults to 1000; `parse-cache-size` and `parse-cache-evict-if-needed` manage the bound. Test added in `tests/core/test-parser.lisp`.

---

## Architecture

7. **Unified 3-level optimization pipeline implemented** (2026-06-28)
   - **Description**: `configure-optimization` in `src/core/pipeline.lisp` provides a single API to enable the three levels of self-modification: Level 1 (runtime cache), Level 2 (operator specialization), and Level 3 (source-level rewriting + automatic persistence). A cache-set hook drives Level 2 specialization and Level 3 rewrites automatically.
   - **Impact**: Users can now enable all optimization levels with one call instead of manually configuring each mechanism.
   - **Resolution**: Created `src/core/pipeline.lisp` with `configure-optimization`, added it to `self-modifying-calculator.asd`, and exported the public symbols. Test added in `tests/integration/test-integration.lisp`.

8. **Function call syntax implemented in the parser** (2026-06-28)
   - **Description**: The string parser now supports function-call syntax such as `sin(x)`, `dot(a,b)`, and `vec3(x,y,z)`. Identifiers can include digits (e.g., `vec3`).
   - **Impact**: CLI users can now invoke trig, vector, and statistics functions with natural math syntax.
   - **Resolution**: `parse-primary` in `src/interface/parser.lisp` recognizes keyword tokens followed by `:lparen`, parses comma-separated arguments, and builds the corresponding AST. Tests added in `tests/core/test-parser.lisp`.

9. **Self-writer output moved out of the source tree** (2026-06-28)
   - **Description**: `write-cache-as-source` now writes to `cache/generated/cache-literals.lisp` instead of `src/generated/cache-literals.lisp`. A `.gitignore` file has been added to exclude generated cache files and compiled Lisp files.
   - **Impact**: Generated code is no longer mixed with hand-written source code, reducing repository pollution risk.
   - **Resolution**: Updated `*generated-cache-file*` in `src/core/self-writer.lisp`; created `.gitignore`.

---

## Documentation

10. **README test command simplified** (2026-06-28)
    - **Description**: The README test command now loads a single `tests/load-all-tests.lisp` helper.
    - **Impact**: Test invocation is simpler and easier to maintain.
    - **Resolution**: `tests/load-all-tests.lisp` loads all core, math, and integration tests and calls `run-all-tests`. README updated.

11. **No demo script** (2026-06-29)
    - **Description**: New users had no single-command way to see the calculator win.
    - **Impact**: The project was harder to demonstrate without running the full benchmark suite.
    - **Resolution**: `demo.lisp` runs a focused realistic renderer benchmark and prints conventional time, SMC time, speedup, and cache stats.

12. **Package exports are incomplete** (2026-06-28)
    - **Description**: Many internal functions used by tests and benchmarks were historically accessed with `smc::` double-colon notation.
    - **Impact**: README examples use double-colon access, which is non-idiomatic.
    - **Planned resolution**: Export commonly used internal functions (partially done in this session; verify and update remaining examples).

13. **No integration tests** (2026-06-28)
    - **Description**: Tests are unit-level only. There are no end-to-end tests covering parse → evaluate → cache → save/load workflows.
    - **Impact**: Risk of regressions in combined workflows not caught by unit tests.
    - **Planned resolution**: Add `tests/integration/test-integration.lisp` with end-to-end scenarios.

14. **`plan.md` factual drift resolved** (2026-06-28)
    - **Description**: `plan.md` previously mentioned Quicklisp, a Pratt parser, the FiveAM/Prove testing framework, a separate `cli.lisp`, a 1,000,000-calculation long series, and mismatched phase numbering. These were corrected in this session.
    - **Impact**: Documentation now matches the implemented state.
    - **Resolution**: Completed. Continue to keep `plan.md` in sync with future changes.

---

*This document is updated whenever new limitations are discovered or resolved.*
