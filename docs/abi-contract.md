# SMC ABI Contract v1

> **Version**: 1  
> **Date**: 2026-07-02  
> **Scope**: This document is the formal contract for the C application-binary interface of the Self-Modifying Calculator (SMC). It is intended for authors of C/C++ host programs, language bindings, and generated dispatch tables.

---

## 1. Overview

SMC exposes a minimal C API with two tiers:

- **Tier 1 — Tooling**: expression-string evaluation (`smc_eval_*`).  Parses a string on every call.  Useful for development, prototyping, and fallback paths, but not for frame-budgeted hot loops.
- **Tier 2 — Production**: generated-code dispatch by stable expression ID (`smc_call_*`).  No string parsing, no heap allocation, deterministic performance.  This is the path intended for games and simulations.

This contract defines what is stable across releases, what is private, and what behavior callers can rely on.

---

## 2. Versioning

### 2.1 ABI version

`SMC_ABI_VERSION` is a compile-time integer constant exposed in `include/smc.h`. It is bumped only when a release changes one of the following in a backward-incompatible way:

- The layout of a public struct (`smc_error_t`).
- The calling convention or signature of a public function.
- The semantics of a public function (e.g., changing null-pointer behavior).

The runtime exposes its ABI version via `smc_abi_version()`. Generated dispatch tables embed their ABI version via `SMC_GENERATED_ABI_VERSION`. `smc_init()` returns `SMC_ERR_ABI` if the linked generated table's ABI version does not match the runtime's ABI version.

### 2.2 Source compatibility

Adding new functions to the header is a backward-compatible change. Removing functions, changing signatures, or changing public struct layout is a breaking change and requires bumping `SMC_ABI_VERSION`.

---

## 3. Struct Layout

### 3.1 Public struct: `smc_error_t`

```c
struct smc_error {
    int   code;
    char  message[256];
};
```

This layout is frozen for ABI v1. Host code may read `code` and `message` directly. `message` is always NUL-terminated.

### 3.2 Opaque struct: `smc_context_t`

`smc_context_t` is an incomplete type in the public header. Its layout is private and may change without bumping the ABI version as long as the public API remains source- and binary-compatible.

---

## 4. Calling Convention

All public functions use C linkage and the platform's default C calling convention. Functions return an `int` status code:

- `0` (`SMC_OK`) on success.
- A negative error code on failure.

Output values are written through pointer arguments. Required pointer arguments must be non-NULL; otherwise the function returns `SMC_ERR_INVALID`.

---

## 5. Ownership & Lifetimes

| Entity | Allocation | Lifetime | Destroy function |
|--------|-----------|----------|------------------|
| `smc_context_t*` | Heap | Until `smc_context_destroy` | `smc_context_destroy` |
| `const char*` from `smc_expr_source` | Static (generated table) | Process lifetime | None |
| `const smc_error_t*` from `smc_last_error*` | Static/thread-local | Until next call modifying the same error slot | None |

No public function except `smc_context_create` returns heap memory. There is no `smc_free`.

---

## 6. Null-Pointer Behavior

Functions that accept required pointers return `SMC_ERR_INVALID` if a required pointer is `NULL`. Optional pointers are documented per function. For example:

- `smc_eval_double(NULL, &out)` returns `SMC_ERR_INVALID`.
- `smc_context_destroy(NULL)` is a no-op.

---

## 7. Pre-Initialization Rules

Before `smc_init()` returns successfully, the only safe functions are:

- `smc_init()`
- `smc_abi_version()`
- `smc_runtime_kind()`
- `smc_error_string()`

All other functions return `SMC_ERR_INIT` if called before the library is initialized.

After `smc_shutdown()` returns, the same pre-init-safe functions remain available.

---

## 8. Thread Safety

### 8.1 Generated hot path

`smc_call_double`, `smc_call_float`, and `smc_call_int` are stateless and read-only. They are safe to call concurrently from multiple threads, provided the generated dispatch table is immutable.

### 8.2 Global context

The implicit global context (`smc_eval_*`, `smc_set_variable_double`, `smc_cache_*`, etc.) is single-threaded by default. Build with `SMC_THREAD_SAFE=ON` to enable internal locking, or use per-thread `smc_context_t*` instances.

### 8.3 Lifecycle

`smc_init` and `smc_shutdown` are not thread-safe and must be called once per process, from a single thread.

---

## 9. Error Codes

| Code | Name | Meaning |
|------|------|---------|
| 0 | `SMC_OK` | Success |
| -1 | `SMC_ERR_INIT` | Library not initialized or initialization failed |
| -2 | `SMC_ERR_PARSE` | Expression could not be parsed |
| -3 | `SMC_ERR_EVAL` | Expression evaluated to an error |
| -4 | `SMC_ERR_NOT_IMPL` | Feature not implemented in this runtime |
| -5 | `SMC_ERR_IO` | File or I/O error |
| -6 | `SMC_ERR_INVALID` | Invalid argument (e.g., null pointer) |
| -7 | `SMC_ERR_ABI` | ABI version mismatch |
| -8 | `SMC_ERR_ARITY` | Wrong number of arguments for expression ID |
| -9 | `SMC_ERR_NOT_FOUND` | Expression ID not present in generated table |
| -10 | `SMC_ERR_THREAD` | Thread-safety violation |
| -11 | `SMC_ERR_SHUTDOWN` | Library has been shut down |

---

## 10. Tier 1 vs. Tier 2

### 10.1 Tier 1 — Tooling

`smc_eval_*` functions parse an expression string on every call. They are intended for:

- Development and prototyping.
- Unit tests and benchmarks.
- Fallback paths where a new expression must be evaluated once.

They are explicitly not recommended for frame-budgeted hot loops.

### 10.2 Tier 2 — Production

`smc_call_*` functions evaluate a pre-generated expression by stable ID. They are intended for:

- Hot loops in games, renderers, and simulations.
- Any path where the same mathematical expression is evaluated many times with different scalar inputs.

---

## 11. Generated Dispatch Table Contract

A generated dispatch table (e.g., `smc_generated.c`) must:

- Define `SMC_GENERATED_ABI_VERSION` equal to `SMC_ABI_VERSION` of the runtime it is built against.
- Define `SMC_GENERATED_EXPR_COUNT` as the number of expressions.
- Provide `smc_call_*`, `smc_expr_count`, `smc_expr_arity`, and `smc_expr_source` with strong symbol definitions that override the runtime's weak fallbacks.
- Store expression source strings in static storage with process lifetime.
- Be immutable after initialization.

The runtime may reject a generated table at `smc_init` time if the ABI version does not match.

---

## 12. Compatibility Checklist for Host Authors

- [ ] Call `smc_init()` once before any other API.
- [ ] Use `smc_call_*` for repeated hot-path evaluations.
- [ ] Use `smc_eval_*` only for tooling or one-off evaluation.
- [ ] Treat `smc_context_t*` as opaque.
- [ ] Do not write to `smc_error_t` memory returned by `smc_last_error*`.
- [ ] Do not free pointers returned by `smc_expr_source` or `smc_last_error*`.
- [ ] Use per-thread contexts or build with `SMC_THREAD_SAFE=ON` for multi-threaded Tier 1 use.
- [ ] Check return codes; do not assume `SMC_OK`.

---

## 13. Related Documents

- `include/smc.h` — the canonical C header.
- `docs/c-api-maturity-plan.md` — the implementation plan that produced this contract.
- `docs/integration-guide.md` — integration steps for C and Python.
