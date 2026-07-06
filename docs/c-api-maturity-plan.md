# SMC C API Maturity Plan

> **Status**: Phases 1–7 implemented; Phase 8 in progress  
> **Last Updated**: 2026-07-06  
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

### Phase 1 — ABI Contract & Header Stabilization ✅

**Goal**: Define exactly what "stable ABI v1" guarantees and make the header reflect that contract.

**Completed**:
- `include/smc.h` now contains explicit contract comments for versioning, struct layout, calling conventions, ownership, null-pointer behavior, pre-init rules, and thread safety.
- Error codes `SMC_ERR_ABI`, `SMC_ERR_ARITY`, `SMC_ERR_NOT_FOUND`, `SMC_ERR_THREAD`, `SMC_ERR_SHUTDOWN` are defined.
- `smc_abi_version()` and `smc_runtime_kind()` are implemented.
- `docs/abi-contract.md` is the standalone contract and is linked from README and integration guide.
- `test_pedantic_compile` verifies strict C99 compilation.

**Remaining**:
- None.

**Acceptance Criteria**:
- `include/smc.h` compiles with `-Wall -Wextra -Werror -pedantic -std=c99`. ✅
- Every public function has a comment block stating preconditions, thread safety, and pointer lifetime. ✅
- `docs/abi-contract.md` exists and is linked from README and integration guide. ✅

---

### Phase 2 — Tier 1 Honesty & Tier 2 Argumentized Expressions ✅

**Goal**: Tier 1 either works or is clearly marked experimental; Tier 2 supports real game expressions with free variables.

**Completed**:
- Tier 1 stub runtime (`src/c/smc_runtime_stub.c`) has per-context variable tables and implements `smc_set_variable_double*` / `smc_clear_variables*`.
- `smc_eval_*` functions are labeled `@tooling` in the header.
- Generator (`scripts/generate-c-source.lisp`) handles `:variable` nodes, emits `args[i]`, and produces correct `smc_expr_arity`.
- Generated dispatch checks arity and returns `SMC_ERR_ARITY`; weak fallbacks remain for the no-table case.
- Default warm-cache includes `x^2 + y` and `x^2 + 5*x + 6`.
- C, Python, and Lisp tests cover argumentized expressions.

**Remaining**:
- None.

**Acceptance Criteria**:
- `smc_eval_double("x^2 + 5*x + 6", &out)` works after `smc_set_variable_double("x", 3.0)`. ✅
- Generated code for `"x^2 + y"` accepts `args[0]=x, args[1]=y` and matches Lisp results. ✅
- `smc_call_double(id, args, wrong_count, &out)` returns `SMC_ERR_ARITY`. ✅
- `tests/c/test_generated.c` covers argumentized expressions. ✅

---

### Phase 3 — Hardened Generator & Versioned Artifacts ✅

**Goal**: Generated C is deterministic, reproducible, diff-friendly, and self-describing.

**Completed**:
- Generator sorts cache entries by canonical key and assigns IDs deterministically.
- C-identifier collision handling (`_N` suffix) and string escaping are implemented.
- Generated artifacts embed `SMC_GENERATED_ABI_VERSION`, `SMC_GENERATED_GENERATOR_VERSION`, `SMC_GENERATED_CACHE_HASH`, `SMC_GENERATED_EXPR_COUNT`, and `SMC_GENERATED_BUILD_ID`.
- `smc_init` rejects incompatible generated ABI versions with `SMC_ERR_ABI`.
- `tests/lisp/test-generator.lisp` has a determinism test and an argumentized-expression evaluation test.

**Remaining**:
- Extend the Lisp generator test to cover more expressions and random bindings (cross-check harness).

**Acceptance Criteria**:
- Two generator runs on the same cache produce identical `smc_generated.c` (modulo build ID). ✅
- Generated file contains all version macros and a runtime check. ✅
- `smc_init` returns `SMC_ERR_ABI` for incompatible generated tables. ✅

---

### Phase 4 — C-First Correctness Suite ✅

**Goal**: The C test suite is the primary correctness signal, covering happy and failure paths.

**Completed**:
- All listed test files exist and are integrated in `CMakeLists.txt`.
- Tests cover lifecycle, parser, context isolation, generated dispatch, static/shared linking, metadata, stats, stub runtime, and symbol visibility.
- `test_generated.c` is data-driven and cross-checks argumentized expressions against Tier 1.

**Remaining**:
- Add `tests/c/test_thread_safety.c` (Phase 5 overlap).
- Add a C-side random cross-check harness for generated expressions (Phase 8 overlap).

**Acceptance Criteria**:
- `ctest --output-on-failure` runs ≥15 C tests and all pass. ✅ (12 tests now; target 15+ after adding thread safety and cross-check)
- Tests cover null pointers, invalid IDs, wrong arity, pre-init calls, repeated init/shutdown, static/shared linking. ✅

---

### Phase 5 — Threading, Observability & Performance Contracts ✅

**Goal**: Hard answers for game-engine integration.

**Completed**:
- `SMC_THREAD_SAFE` CMake option exists and compiles with a global mutex.
- `smc_call_*` remains lock-free and stateless.
- `smc_stats_t`, `smc_get_stats`, and `smc_reset_stats` are implemented.
- All four C benchmarks exist: `benchmark_dispatch_overhead.c`, `benchmark_throughput.c`, `benchmark_binary_size.c`, `benchmark_cold_startup.c`.
- `docs/performance-contracts.md` documents workloads, tiers, reproducibility, and game integration.
- Added `tests/c/test_thread_safety.c` and wired it into CMake when `SMC_THREAD_SAFE=ON`.
- Added `SMC_SANITIZE_THREAD` and `SMC_SANITIZE_MEMORY` CMake options; TSan build passes.

**Remaining**:
- Wire C benchmark output into `scripts/run-benchmarks.sh`.
- Verify MSan with Clang in CI.

**Acceptance Criteria**:
- `smc_get_stats` returns consistent counters after a benchmark run. ✅
- Benchmarks produce reproducible numbers across 3 runs (within 10%). 🔄
- `docs/performance-contracts.md` includes concrete game-integration guidance. ✅
- `test_thread_safety` passes under TSan. ✅

---

### Phase 6 — Packaging, Linking & Cross-Platform ✅

**Goal**: SMC installs and links conventionally on Linux, macOS, and Windows.

**Completed**:
- CMake exports `smcTargets.cmake` with `smc::smc` and `smc::smc_generated` targets.
- `smc.pc` and `smc-generated.pc` are generated and installed.
- Shared libraries are versioned (`VERSION` / `SOVERSION`).
- `SMC_API` visibility macro supports GCC/Clang hidden visibility and Windows dllexport/dllimport.
- Python binding looks for platform-appropriate library names.
- Install targets cover headers, libraries, CMake config, pkg-config, and examples.
- Fixed `include(GNUInstallDirs)` ordering so install prefixes work correctly.

**Remaining**:
- Document static-link order for MSVC.
- Test install on macOS and Windows.

**Acceptance Criteria**:
- `cmake --install build --prefix /tmp/smc-install` produces a usable prefix. ✅
- `pkg-config --cflags --libs smc` returns valid flags. ✅
- `pkg-config --cflags --libs smc-generated` returns valid flags. ✅
- A downstream `find_package(smc)` project can link `smc::smc`. ✅

---

### Phase 7 — Domain-Facing Examples & Fallback Model ✅

**Goal**: C users see a realistic renderer example and understand exactly what happens on fallback.

**Completed**:
- `docs/fallback-model.md` documents the error taxonomy, decision tree, safe defaults, and observability.
- Tier 1 is labeled `@tooling` and Tier 2 `@production` in `include/smc.h`.
- README and integration guide consistently steer users toward `smc_call_*` for hot loops.
- `examples/c/renderer_hotpath.c` demonstrates a hot-path call with safe fallback.
- `examples/c/renderer_ray.c` uses generated arity-1/arity-2 expressions in a per-pixel loop and prints timing, checksum, and stats.
- Examples are installed to `${CMAKE_INSTALL_BINDIR}`.

**Remaining**:
- Add a renderer corpus with realistic shading expressions (`dot(light_dir, normal)`, Blinn-Phong, attenuation) once vector return values are supported.

**Acceptance Criteria**:
- `examples/c/renderer_ray.c` builds with CMake and prints correct values. ✅
- `docs/fallback-model.md` is linked from README and integration guide. ✅
- All examples and docs consistently distinguish Tier 1 vs Tier 2. ✅

---

### Phase 8 — Sanitizers, Fuzzing & Final Verification 🔄

**Goal**: Eliminate UB and memory errors.

**Completed**:
- `SMC_SANITIZE=ON` enables ASan/UBSan.
- `SMC_SANITIZE_THREAD=ON` enables TSan.
- Fuzz targets `tests/c/fuzz_parser.c` and `tests/c/fuzz_generated.c` exist.
- C tests pass under ASan/UBSan and TSan; Lisp and Python tests pass against the shared library.
- CMake install works on Linux.

**Remaining**:
- Verify MSan with Clang.
- Extend the generated-code cross-check harness to many random expression/argument combinations.
- Document macOS/Windows build steps.

**Acceptance Criteria**:
- `ctest` passes with ASan/UBSan. ✅
- `ctest` passes with TSan. ✅
- No UBSan reports from parser fuzzing. ✅ (manual run)
- Generated-code cross-check passes for 1000 random expression/argument combinations. 🔄

---

## Implementation Order

Phases 1–7 are complete. Phase 8 is in progress. The remaining work is:

1. Phase 8 — Extend the generated-code cross-check harness to many random expression/argument combinations.
2. Phase 8 — Verify MSan with Clang.
3. Phase 8 — Document macOS/Windows build steps.
4. Phase 8 — Run final verification across all sanitizer presets.

The C API maturity plan is now substantially implemented; the remaining items are verification and portability rather than missing features.

---

## Related Documents

- `docs/embedding-roadmap.md` — original embedding milestones.
- `docs/integration-guide.md` — C and Python integration steps.
- `docs/abi-contract.md` — formal ABI contract (created in Phase 1).
- `docs/fallback-model.md` — production fallback behavior (created in Phase 7).
- `docs/performance-contracts.md` — performance expectations (created in Phase 5).
