# SMC C API Maturity Plan

> **Status**: Active plan — approach one phase at a time  
> **Last Updated**: 2026-07-02  
> **Scope**: Evolve the SMC C API from a well-shaped research prototype into a production-quality, embeddable adaptive computation library for games and simulations.

This document is the authoritative plan for C API maturity. It supersedes the embedding roadmap for C-specific work and should be implemented in individual phases. Each phase is designed to be reviewed independently before moving to the next.

---

## Constraints & Decisions

The following decisions are fixed for this plan:

| Decision | Rationale |
|----------|-----------|
| `src/c/smc_runtime_sbcl.c` remains a placeholder | Wiring `sb-alien` callbacks adds complexity before the C ABI is proven. The SBCL-backed runtime stays a documented experimental path. |
| Scalar-only ABI for v1 | Keeps the ABI minimal and stable. Vector/matrix support will arrive later as domain adapters after the generated scalar path is mature. |
| Global context is single-threaded by default | No mutex overhead in the default hot path. Build with `SMC_THREAD_SAFE=ON` for internal locking, or use explicit `smc_context_t*` instances. |
| Generated `smc_call_*` must be stateless/read-only | Concurrent calls are safe as long as the generated dispatch table is immutable. |

---

## Goals

The plan addresses the following gaps identified in the current C API:

1. Define a formal ABI contract.
2. Finish Tier 1 honestly or mark it as tooling-only.
3. Make Tier 2 support non-ground (argumentized) expressions.
4. Harden the C code generator.
5. Add a C-first correctness suite covering failure paths.
6. Solve linking and packaging robustly.
7. Define thread behavior formally.
8. Add performance contracts and benchmarks.
9. Add domain-facing C examples.
10. Decide and implement a mature fallback model.
11. Add observability counters.
12. Freeze naming and layering.
13. Make memory ownership explicit.
14. Run sanitizers and fuzzers.
15. Version generated artifacts.

---

## Phases

### Phase 1 — ABI Contract & Header Stabilization

**Goal**: Define exactly what "stable ABI v1" guarantees and make the header reflect that contract.

**Deliverables**:
- Rewrite `include/smc.h` with explicit contracts:
  - `SMC_ABI_VERSION` semantics and runtime rejection of incompatible generated artifacts.
  - Public struct layout (`smc_error_t`) vs. opaque pointers (`smc_context_t`).
  - Calling conventions and ownership rules.
  - `const char*` lifetimes (e.g., `smc_expr_source` returns static generated storage).
  - Null-pointer behavior for every function.
  - Pre-`smc_init` rules: only `smc_init` and `smc_error_string` are safe before init.
- Add new error codes: `SMC_ERR_ABI`, `SMC_ERR_ARITY`, `SMC_ERR_NOT_FOUND`, `SMC_ERR_THREAD`, `SMC_ERR_SHUTDOWN`.
- Add `smc_abi_version()` and `smc_runtime_kind()` introspection functions.
- Create `docs/abi-contract.md` as the standalone ABI contract.
- Update `README.md` and `docs/integration-guide.md` to reference the contract.

**Acceptance Criteria**:
- `include/smc.h` compiles with `-Wall -Wextra -Werror -pedantic -std=c99`.
- Every public function has a comment block stating preconditions, thread safety, and pointer lifetime.
- `docs/abi-contract.md` exists and is linked from README and integration guide.

---

### Phase 2 — Tier 1 Honesty & Tier 2 Argumentized Expressions

**Goal**: Tier 1 either works or is clearly marked experimental; Tier 2 supports real game expressions with free variables.

**Deliverables**:
- Complete Tier 1 stub runtime (`src/c/smc_runtime_stub.c`):
  - Implement variable binding with a small per-context variable table.
  - Implement `smc_set_variable_double*`, `smc_clear_variables*`.
  - Keep cache persistence and source generation returning `SMC_ERR_NOT_IMPL`, but document them as requiring SBCL or generated build.
  - Mark `smc_eval_*` in the header as `@tooling` / experimental.
- Extend the generator (`scripts/generate-c-source.lisp`) for non-ground expressions:
  - Handle `:variable` nodes.
  - Assign deterministic argument order per expression (left-to-right variable occurrence, deduplicated).
  - Emit generated wrappers that substitute `args[i]` for variable nodes.
  - Emit correct `smc_expr_arity` per expression.
  - Cross-check generated expressions against the Lisp evaluator with sample bindings.
- Update generated dispatch (`src/c/smc_generated_runtime.c` + generated output):
  - Add arity checking: wrong `argc` returns `SMC_ERR_ARITY`.
  - Keep weak fallbacks for the no-generated-table case.

**Acceptance Criteria**:
- `smc_eval_double("x^2 + 5*x + 6", &out)` works after `smc_set_variable_double("x", 3.0)`.
- Generated code for `"x^2 + y"` accepts `args[0]=x, args[1]=y` and matches Lisp results.
- `smc_call_double(id, args, wrong_count, &out)` returns `SMC_ERR_ARITY`.
- `tests/c/test_generated.c` covers argumentized expressions.

---

### Phase 3 — Hardened Generator & Versioned Artifacts

**Goal**: Generated C is deterministic, reproducible, diff-friendly, and self-describing.

**Deliverables**:
- Deterministic generator (`scripts/generate-c-source.lisp`):
  - Replace hash-table traversal with sorted canonical keys.
  - Assign IDs from a deterministic sequence.
  - Configurable symbol prefix (default `smc_expr_`).
  - C-identifier collision handling (`_N` suffix).
  - String escaping rules for source literals and comments.
  - Duplicate-expression handling (same AST string with different arity gets different IDs).
  - Reproducible output: sorted `#define`s, sorted switch cases, stable formatting.
- Versioned generated artifacts:
  - Embed `SMC_GENERATED_ABI_VERSION`, `SMC_GENERATED_GENERATOR_VERSION`, `SMC_GENERATED_CACHE_HASH`, `SMC_GENERATED_EXPR_COUNT`, and optional `SMC_GENERATED_BUILD_ID`.
  - Runtime `smc_init` rejects incompatible generated ABI versions.
- Generator tests:
  - `tests/lisp/test-generator.lisp` asserts byte-identical output across two runs.
  - Property tests compare generated C results with Lisp evaluator for random expressions and bindings.

**Acceptance Criteria**:
- Two generator runs on the same cache produce identical `smc_generated.c` (modulo build ID).
- Generated file contains all version macros and a runtime check.
- `smc_init` returns `SMC_ERR_ABI` for incompatible generated tables.

---

### Phase 4 — C-First Correctness Suite

**Goal**: The C test suite is the primary correctness signal, covering happy and failure paths.

**Deliverables**:
- New test files:
  - `tests/c/test_lifecycle.c` — init/shutdown, double init, pre-init calls, context create/destroy, null context.
  - `tests/c/test_parser.c` — precedence, unary, whitespace, malformed input, division by zero, NaN/overflow handling.
  - `tests/c/test_context_isolation.c` — variable and optimization-level isolation.
  - `tests/c/test_generated_dispatch.c` — valid IDs, invalid IDs, wrong arity, null args/out, cross-check against Lisp.
  - `tests/c/test_static_link.c` and `tests/c/test_shared_link.c` — verify symbol resolution for each library form.
  - `tests/c/test_metadata.c` — expression metadata and error handling.
- CMake integration:
  - Add all tests under `SMC_BUILD_TESTS`.
  - Add `test_static_link` and `test_shared_link` targets with correct link ordering.

**Acceptance Criteria**:
- `ctest --output-on-failure` runs ≥15 C tests and all pass.
- Tests cover null pointers, invalid IDs, wrong arity, pre-init calls, repeated init/shutdown, static/shared linking.

---

### Phase 5 — Threading, Observability & Performance Contracts

**Goal**: Hard answers for game-engine integration.

**Deliverables**:
- Threading model:
  - Add `SMC_THREAD_SAFE` CMake option.
  - When ON, protect the global context with a mutex; when OFF, document single-threaded requirement.
  - Keep `smc_call_*` lock-free and stateless.
  - Update `docs/integration-guide.md` threading section.
- Observability API:
  - Add `smc_stats_t` struct with counters: total calls, generated hits, fallback evals, invalid IDs, arity errors, parse errors, last error code.
  - Add `smc_get_stats(smc_stats_t *out)` and `smc_reset_stats()`.
- Performance benchmarks:
  - `benchmark_dispatch_overhead.c` — `smc_call_double` vs. inline C.
  - `benchmark_throughput.c` — throughput per expression ID.
  - `benchmark_binary_size.c` — `.text` growth per generated expression.
  - `benchmark_cold_startup.c` — init-to-first-call latency.
  - `docs/performance-contracts.md` documenting when SMC wins and when overhead dominates.

**Acceptance Criteria**:
- `smc_get_stats` returns consistent counters after a benchmark run.
- Benchmarks produce reproducible numbers across 3 runs (within 10%).
- `docs/performance-contracts.md` includes concrete game-integration guidance.

---

### Phase 6 — Packaging, Linking & Cross-Platform

**Goal**: SMC installs and links conventionally on Linux, macOS, and Windows.

**Deliverables**:
- CMake package config:
  - Generate `smcConfig.cmake`, `smcConfigVersion.cmake`, `smcTargets.cmake`.
  - Support `find_package(smc)` with `smc::smc` and `smc::smc_generated` targets.
- pkg-config:
  - Generate and install `smc.pc`.
- Versioned shared libraries:
  - Set `VERSION` and `SOVERSION` on shared targets.
- Symbol visibility:
  - Add `SMC_API` macro for `visibility("default")` / `dllexport` / `dllimport`.
  - Build with `-fvisibility=hidden` on GCC/Clang.
- Windows/macOS support:
  - Platform-appropriate library names in CMake and Python binding.
  - Document static-link order for MSVC.
- Install targets for headers, libraries, CMake config, pkg-config, and examples.

**Acceptance Criteria**:
- `cmake --install build --prefix /tmp/smc-install` produces a usable prefix.
- `pkg-config --cflags --libs smc` returns valid flags.
- A downstream `find_package(smc)` project can link `smc::smc`.

---

### Phase 7 — Domain-Facing Examples & Fallback Model

**Goal**: C users see a realistic renderer example and understand exactly what happens on fallback.

**Deliverables**:
- Ray/renderer example (`examples/c/renderer_ray.c`):
  - Real expression keys: `dot(light_dir, normal)`, `blinn_phong(...)`, `attenuation(...)`.
  - Warm cache in SBCL, generate C, call by ID in a per-pixel loop.
  - Show fallback behavior for unknown expressions.
  - Print benchmark numbers and cache stats.
- Fallback model documentation (`docs/fallback-model.md`):
  - Distinct error codes for: no generated table, bad ID, wrong arity, unsupported type, uninitialized runtime.
  - Decision tree for production C.
  - Tier 1 explicitly labeled as development/tooling, not for frame hot loops.
- Freeze naming/layering:
  - Header comments label Tier 1 as `@tooling` and Tier 2 as `@production`.
  - Update README, integration guide, and examples to steer users toward `smc_call_*`.

**Acceptance Criteria**:
- `examples/c/renderer_ray.c` builds with CMake and prints correct values.
- `docs/fallback-model.md` is linked from README and integration guide.
- All examples and docs consistently distinguish Tier 1 vs Tier 2.

---

### Phase 8 — Sanitizers, Fuzzing & Final Verification

**Goal**: Eliminate UB and memory errors.

**Deliverables**:
- Sanitizer builds:
  - ASan/UBSan preset.
  - TSan preset for global-context tests (when `SMC_THREAD_SAFE=ON`).
  - MSan if feasible.
- Fuzz targets:
  - `tests/c/fuzz_parser.c` and `tests/c/fuzz_generated.c`.
  - Run with libFuzzer or AFL in CI for a bounded time.
- Cross-check harness:
  - Evaluate every generated expression with random args in both C and Lisp and compare within tolerance.
- Final verification:
  - All C tests pass under ASan/UBSan.
  - All Lisp tests still pass.
  - Python tests pass against the new shared library.
  - CMake install works on Linux; macOS/Windows documented.

**Acceptance Criteria**:
- `ctest` passes with ASan/UBSan.
- No UBSan reports from parser fuzzing.
- Generated-code cross-check passes for 1000 random expression/argument combinations.

---

## Implementation Order

1. Phase 1 — ABI contract & header stabilization.
2. Phase 2 — Tier 1 completion & Tier 2 argumentized expressions.
3. Phase 3 — Hardened generator & versioned artifacts.
4. Phase 4 — C-first correctness suite.
5. Phase 5 — Threading, observability & performance contracts.
6. Phase 6 — Packaging & cross-platform linking.
7. Phase 7 — Domain examples & fallback model.
8. Phase 8 — Sanitizers, fuzzing & final verification.

Each phase should be implemented and reviewed before the next begins.

---

## Related Documents

- `docs/embedding-roadmap.md` — original embedding milestones.
- `docs/integration-guide.md` — C and Python integration steps.
- `docs/abi-contract.md` — formal ABI contract (created in Phase 1).
- `docs/fallback-model.md` — production fallback behavior (created in Phase 7).
- `docs/performance-contracts.md` — performance expectations (created in Phase 5).
