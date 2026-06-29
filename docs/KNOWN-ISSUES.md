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
   - **Description**: Short (100) and medium (10,000) calculation series complete too quickly for reliable wall-clock measurement with the current timer. Many timings report 0.0000 s.
   - **Impact**: Short/medium speedup numbers are unreliable and sometimes report `:inf` speedup.
   - **Planned resolution**: Accumulate multiple repetitions per pass and divide by the repetition count to get measurable elapsed time. This is already done for short/medium in `benchmark-framework.lisp` but may need tuning.

---

## Cache & Memory

4. **Cache eviction policy implemented** (2026-06-28)
   - **Description**: `*global-cache*` and per-cache instances can be created with a positive `max-size`. When the cache is full, the least recently used entry is evicted on the next `cache-set` of a new key.
   - **Impact**: Long-running sessions can now bound memory usage.
   - **Resolution**: `make-cache` accepts an optional `max-size` argument; `cache-touch`, `cache-evict-lru`, and updated `cache-set`/`cache-get` implement LRU semantics. Test added in `tests/core/test-cache.lisp`.

5. **Cache persistence is not automatic** (2026-06-28)
   - **Description**: `save-cache` and `load-cache` exist, but the user must call them manually. The self-writer can write `src/generated/cache-literals.lisp`, but no automatic persistence hook exists on startup or shutdown.
   - **Impact**: Every new session starts with a cold cache unless the user manually persists and reloads.
   - **Planned resolution**: Add optional automatic save/load on process exit/startup controlled by a configuration flag.

6. **Parser cache never cleared** (2026-06-28)
   - **Description**: `*parse-cache*` maps input strings to parsed ASTs and grows without bound. `clear-parse-cache` exists but is not called automatically.
   - **Impact**: Long-running sessions with many distinct input strings may consume memory.
   - **Planned resolution**: Add a size-bound or periodic eviction policy for `*parse-cache*`, or clear it after each CLI invocation.

---

## Architecture

7. **No unified 3-level optimization pipeline** (2026-06-28)
   - **Description**: The three levels of self-modification (runtime cache, runtime specialization, source-level rewriting) exist but must be enabled manually. There is no automatic promotion from Level 1 → Level 2 → Level 3.
   - **Impact**: Users must understand the internals to get the full benefit.
   - **Planned resolution**: Design a single configuration API that enables the appropriate levels automatically based on workload characteristics.

8. **No function call syntax in the parser** (2026-06-28)
   - **Description**: The parser supports prefix operators via S-expression ASTs, e.g., `(:sin x)` or `(:dot ...)` when constructed programmatically, but the string parser does not support `sin(x)` or `dot(a,b)` notation.
   - **Impact**: CLI users cannot invoke trig, vector, or statistics functions with natural math syntax.
   - **Planned resolution**: Extend the recursive-descent parser to recognize function identifiers followed by parentheses and comma-separated arguments.

9. **Self-writer writes into the source tree** (2026-06-28)
   - **Description**: `write-cache-as-source` writes to `src/generated/cache-literals.lisp`. The `src/generated/` directory may not exist and generated files inside `src/` can be accidentally committed.
   - **Impact**: Generated code is mixed with hand-written code, risking repository pollution.
   - **Planned resolution**: Establish the generated directory as part of the build, add it to `.gitignore` (or document it as checked-in), and clarify the lifecycle.

---

## Documentation

10. **README test command is cumbersome** (2026-06-28)
    - **Description**: The README test command loads eight separate test files with eight `--eval` expressions.
    - **Impact**: Test invocation is error-prone and hard to maintain.
    - **Planned resolution**: Provide a single `tests/load-all-tests.lisp` helper that loads all core and math tests in one go and update the README command.

11. **Package exports are incomplete** (2026-06-28)
    - **Description**: Many internal functions used by tests and benchmarks were historically accessed with `smc::` double-colon notation.
    - **Impact**: README examples use double-colon access, which is non-idiomatic.
    - **Planned resolution**: Export commonly used internal functions (partially done in this session; verify and update remaining examples).

12. **No integration tests** (2026-06-28)
    - **Description**: Tests are unit-level only. There are no end-to-end tests covering parse → evaluate → cache → save/load workflows.
    - **Impact**: Risk of regressions in combined workflows not caught by unit tests.
    - **Planned resolution**: Add `tests/integration/test-integration.lisp` with end-to-end scenarios.

13. **`plan.md` factual drift resolved** (2026-06-28)
    - **Description**: `plan.md` previously mentioned Quicklisp, a Pratt parser, the FiveAM/Prove testing framework, a separate `cli.lisp`, a 1,000,000-calculation long series, and mismatched phase numbering. These were corrected in this session.
    - **Impact**: Documentation now matches the implemented state.
    - **Resolution**: Completed. Continue to keep `plan.md` in sync with future changes.

---

*This document is updated whenever new limitations are discovered or resolved.*
