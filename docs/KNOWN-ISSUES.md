# Known Issues & Technical Debt

> **Date**: 2026-06-28
> **Purpose**: Track all known gaps, limitations, and technical debt items. Each entry includes the date discovered, a description, impact, and any planned or completed resolution.

---

## Performance

1. **Performance targets not met** (2026-06-28)
   - **Description**: Plan targets 5.0× speedup for long series and 20× stretch. Current actual LONG series (100,000 calculations) results are 1.17× arithmetic, 3.33× dot product, 3.00× cross product, 1.20× trig, 3.50× polynomial, 2.67× mixed, 3.25× realistic renderer.
   - **Impact**: The calculator beats conventional evaluation on realistic workloads but falls short on cheap arithmetic.
   - **Planned resolution**: Continue optimizing the dispatch path; consider cache eviction to keep hot entries; revisit compiled dispatch with a non-linear structure.

2. **Compiled cache dispatch disabled for large caches** (2026-06-28)
   - **Description**: `src/core/dispatch-compiler.lisp` exists but produces a linear `cond` scan. For medium/large caches the scan is slower than the EQ hash table and causes control-stack exhaustion during compilation for very large caches.
   - **Impact**: The compiled dispatch path cannot be enabled by default.
   - **Planned resolution**: Revisit with a smarter structure (e.g., trie, hash-based dispatch, or limited per-operator dispatch tables) that avoids linear scan and stack overflow.

3. **Short/medium benchmark timing noise** (2026-06-28)
   - **Description**: Short (100) and medium (10,000) calculation series are now measured by repeating each pass 100× and 10× respectively, then dividing by the repetition count. Timings are now measurable, though long series remain the most reliable source of speedup numbers.
   - **Impact**: Short/medium speedup numbers are now mostly reliable, with occasional run-to-run variance.
   - **Resolution**: `run-benchmark-category` in `tests/benchmarks/benchmark-framework.lisp` accumulates repetitions for short and medium series. Tuning may still be needed if variance remains high.

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

7. **No unified 3-level optimization pipeline** (2026-06-28)
   - **Description**: The three levels of self-modification (runtime cache, runtime specialization, source-level rewriting) exist but must be enabled manually. There is no automatic promotion from Level 1 → Level 2 → Level 3.
   - **Impact**: Users must understand the internals to get the full benefit.
   - **Planned resolution**: Design a single configuration API that enables the appropriate levels automatically based on workload characteristics.

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
