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

## C API Maturity

10. **C API maturity plan created** (2026-07-02)
    - **Description**: The C API is well-shaped but not yet production-mature. A new plan in `docs/c-api-maturity-plan.md` tracks eight phases of hardening: ABI contract, Tier 1 honesty, Tier 2 argumentized expressions, deterministic generator, C-first correctness suite, packaging, threading/observability, domain examples, and sanitizers/fuzzing.
    - **Impact**: C API work now has a structured, phase-by-phase roadmap.
    - **Resolution**: Created `docs/c-api-maturity-plan.md`; updated `docs/embedding-roadmap.md` and `README.md` to reference it.
    - **Planned resolution**: Implement phases one at a time, starting with Phase 1 (ABI contract & header stabilization).

11. **README overstates C/Python embedding maturity** (2026-07-06)
    - **Description**: The README claimed all three C/Python embedding milestones were complete. In reality, Milestone 1 (stable ABI v1, stub runtime, Python binding, C generator) is complete; Milestones 2 and 3 are functional scaffolds that still contain important gaps.
    - **Impact**: New users and reviewers could be misled into thinking the production generated-code path is fully mature.
    - **Resolution**: Updated README to mark Milestones 2 and 3 as "In Progress" and added an explicit limitations section describing the remaining gaps.
    - **Planned resolution**: Close the documented gaps (argumentized default generator output, fallback runtime error distinctions, expanded C correctness suite) before marking Milestones 2 and 3 complete.

12. **Default generator warm-cache emits mostly ground expressions** (2026-07-06)
    - **Description**: `scripts/generate-c-source.lisp` warms the cache by calling `smc:run-calculator` on a list of scalar expressions. Expressions containing free variables fail to evaluate and are not cached, so the generated `smc_generated.c` contains only arity-0 expressions. Argumentized expressions can be generated only by manually calling `smc:evaluate` with variable bindings before running the generator.
    - **Impact**: The documented build flow does not produce the argumentized expressions that the Tier 2 API and examples are designed for.
    - **Resolution**: In progress — update `warm-cache` to bind variables and evaluate representative argumentized expressions.
    - **Planned resolution**: Add expressions such as `x^2 + y` and `x^2 + 5*x + 6` to the default warm-cache with explicit variable bindings.

13. **Generated-runtime fallback error model hardened** (2026-07-06)
    - **Description**: `src/c/smc_generated_runtime.c` returned `SMC_ERR_NOT_IMPL` for every Tier 2 call when no generated dispatch table was linked. It did not distinguish "no generated table linked," "unknown expression ID," or "wrong argument count."
    - **Impact**: Host programs could not tell whether they forgot to link a generated table, passed a bad ID, or passed the wrong arity.
    - **Resolution**: Updated the fallback to return `SMC_ERR_INVALID` for null output pointers and keep `SMC_ERR_NOT_IMPL` only for the no-generated-table case. The generated dispatch table itself returns `SMC_ERR_NOT_FOUND` and `SMC_ERR_ARITY`.
    - **Planned resolution**: None remaining for this item.

14. **C/Python embedding tests now cover argumentized expressions** (2026-07-06)
    - **Description**: The default generator warm-cache, the C acceptance tests, the Python acceptance test, and the Lisp generator test were updated to exercise argumentized expressions (`x^2 + y`, `x^2 + 5*x + 6`).
    - **Impact**: The documented build flow now produces real arity-1 and arity-2 expressions, and the tests verify they match Tier 1 evaluation.
    - **Resolution**: Completed.
    - **Planned resolution**: None.

15. **No multi-threaded C test for Tier 1** (2026-07-06) ✅
    - **Description**: `SMC_THREAD_SAFE=ON` compiles the runtime with a global mutex, but there was no C test that spawns multiple threads and exercises `smc_eval_*` or per-thread `smc_context_t*` instances concurrently.
    - **Impact**: Thread-safety regressions in the global context could go undetected.
    - **Resolution**: Added `tests/c/test_thread_safety.c` and wired it into CMake when `SMC_THREAD_SAFE=ON`.
    - **Planned resolution**: None.

16. **Sanitizer presets incomplete** (2026-07-06) 🔄
    - **Description**: `SMC_SANITIZE=ON` only enables AddressSanitizer and UBSan. There is no CMake option for ThreadSanitizer or MemorySanitizer.
    - **Impact**: Data races and uninitialized-memory issues are not caught by the current CMake presets.
    - **Resolution**: Added `SMC_SANITIZE_THREAD` and `SMC_SANITIZE_MEMORY` CMake options. TSan build passes; MSan not exercised because the host compiler is GCC.
    - **Planned resolution**: Verify MSan with Clang in CI.

17. **No domain-facing renderer example** (2026-07-06) ✅
    - **Description**: `examples/c/renderer_hotpath.c` demonstrates a single scalar hot-path expression, but the maturity plan calls for a realistic renderer example with expression keys such as `dot(light_dir, normal)` and Blinn-Phong terms.
    - **Impact**: New C users do not see a realistic game/simulation integration pattern.
    - **Resolution**: Added `examples/c/renderer_ray.c` that uses generated arity-1/arity-2 expressions in a per-pixel loop, prints timing, checksum, and stats.
    - **Planned resolution**: None.

18. **pkg-config does not list generated-table library** (2026-07-06) ✅
    - **Description**: `smc.pc.in` only lists `-lsmc`. Downstream users who want Tier 2 must also link `-lsmc_generated`.
    - **Impact**: Users following `pkg-config --libs smc` will get unresolved Tier 2 symbols.
    - **Resolution**: Added `smc-generated.pc.in` with `-lsmc_generated -lsmc` and install both `.pc` files.
    - **Planned resolution**: None.

19. **Generated-code cross-check harness is minimal** (2026-07-06) 🔄
    - **Description**: The Lisp generator test checks one argumentized expression. The plan calls for a cross-check harness that evaluates many generated expressions with random arguments in both C and Lisp.
    - **Impact**: Generator correctness is not stress-tested across a broad expression/argument matrix.
    - **Resolution**: Extended `tests/lisp/test-generator.lisp` with a cross-check over three argumentized expressions and multiple bindings.
    - **Planned resolution**: Add a C-side random cross-check test and broaden the Lisp expression list.

20. **Python binding lacks stats example/test** (2026-07-06) ✅
    - **Description**: `python/smc/__init__.py` exposes `get_stats()` and `reset_stats()`, but there was no Python test or example that exercises them.
    - **Impact**: The Python stats API could break without detection.
    - **Resolution**: Added `test_stats()` to `tests/python/test_smc.py` and mentioned stats in the Python module docstring.
    - **Planned resolution**: None.

## Documentation

11. **README test command simplified** (2026-06-28)
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

---

## C API — Artifact Cache v2.1 Complete

21. **Per-entry malloc in artifact cache** (2026-07-09) ✅
    - **Description**: `smc_artifact_store` allocated memory for each entry using `malloc`. This was acceptable for the MVP but caused allocation churn in hot paths.
    - **Impact**: Hot path could not guarantee zero-allocation after configuration.
    - **Resolution** (v2.1): Preallocated fixed-size slots at configuration time. Each entry slot is a fixed buffer of `max_key_size + max_value_size`. No mallocs occur in the hot path after `smc_artifact_configure`. Tested with `test_preallocated_storage()` validating 100 sequential stores/retrievals.

22. **Missing collision-eviction stats test** (2026-07-09) ✅
    - **Description**: The test suite did not verify that `evictions` counter increments correctly when store overwrites an existing entry.
    - **Impact**: Eviction behavior was untested; regression could go undetected.
    - **Resolution**: Added `test_preallocated_storage()` and verified stats are correctly incremented in other tests.

---

*This document is updated whenever new limitations are discovered or resolved.*