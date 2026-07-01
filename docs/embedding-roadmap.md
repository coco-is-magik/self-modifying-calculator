# SMC Embedding Roadmap — C & Python Production Integration

> **Status**: Currently active and prioritized plan  
> **Last Updated**: 2026-07-01  
> **Target**: Evolve the Self-Modifying Calculator (SMC) from a Common Lisp research project into a production-quality embeddable adaptive computation library for C and Python applications.

---

## 1. Executive Summary

This document is the authoritative plan for making SMC usable from plain C and Python projects. The strategy is:

- **Primary production target**: Generate C source code from SMC's hot cached expressions. The runtime is plain C with no Lisp dependency.
- **Development / tooling target**: Embed SBCL as a runtime library so artists, designers, and researchers can evaluate arbitrary expressions and warm up the cache before generating C.
- **Python binding**: A pure-Python `ctypes` wrapper with zero build step.
- **ABI stability**: The C API is versioned and minimal. Hot-path functions avoid strings and heap allocation.

The roadmap is staged into three milestones:

1. **Milestone 1**: Stable C API v1, runtime implementations, and Python binding.
2. **Milestone 2**: Production generated-code API (`smc_call_*`) and C code generator.
3. **Milestone 3**: Game/simulation integration package, build-system support, and benchmarks.

---

## 2. Decisions

| Topic | Decision | Rationale |
|-------|----------|-----------|
| Primary C integration | **Generated C source code** from hot cached expressions | Deterministic, ABI-stable, no runtime SBCL dependency, ideal for games |
| Optional runtime path | **Embedded SBCL runtime** via `save-lisp-and-die` + `sb-alien` C callbacks | Full runtime self-modification for development and prototyping |
| C value model (v1) | **Scalar-only**: `double`, `float`, `int64_t` | Minimal ABI; vectors/matrices via later revision or opaque handles |
| Context model | **Global convenience API + explicit `smc_context_t*`** | Easy for demos, safe for multi-tenant or threaded use |
| Python binding | **`ctypes` primary** | No build step, pure Python, easy to debug and install |
| Python API shape | Expression-based: `smc.eval(expr)`, `smc.Context` | Natural calculator usage; `compile()` is a documented future enhancement |
| Milestone 1 runtime | **Both stub runtime (default) and optional SBCL-backed runtime** | C builds work without SBCL; full fidelity available when SBCL is present |

---

## 3. Stable C API v1

### 3.1 Header (`include/smc.h`)

```c
/* smc.h — stable ABI v1 */
#ifndef SMC_H
#define SMC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMC_ABI_VERSION 1

typedef struct smc_context smc_context_t;
typedef struct smc_error  smc_error_t;
typedef uint32_t          smc_expr_id_t;

/* -------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

/* One-time library initialization. Must be called before any other API. */
int smc_init(void);

/* One-time library shutdown. Releases global resources. */
int smc_shutdown(void);

/* Create an isolated context with the given optimization level (1..3). */
smc_context_t *smc_context_create(int level);

/* Destroy a context created with smc_context_create(). */
void smc_context_destroy(smc_context_t *ctx);

/* -------------------------------------------------------------------------- */
/* Global context convenience API                                           */
/* -------------------------------------------------------------------------- */

/* Set the optimization level of the implicit global context. */
int smc_set_optimization_level(int level);

/* Get the optimization level of the implicit global context. */
int smc_get_optimization_level(void);

/* -------------------------------------------------------------------------- */
/* Tier 1: Development / tooling / embedded SBCL — expression strings         */
/* -------------------------------------------------------------------------- */

/* Evaluate a mathematical expression string and write the result to *out. */
int smc_eval_double(const char *expr, double *out);
int smc_eval_double_with(smc_context_t *ctx, const char *expr, double *out);

int smc_eval_float(const char *expr, float *out);
int smc_eval_float_with(smc_context_t *ctx, const char *expr, float *out);

int smc_eval_int(const char *expr, int64_t *out);
int smc_eval_int_with(smc_context_t *ctx, const char *expr, int64_t *out);

/* -------------------------------------------------------------------------- */
/* Tier 2: Production generated-code — expression IDs, no strings             */
/* -------------------------------------------------------------------------- */

/* Call a cached expression by stable ID, passing only its free variables. */
int smc_call_double(smc_expr_id_t expr_id,
                    const double *args, size_t argc,
                    double *out);

int smc_call_float(smc_expr_id_t expr_id,
                   const float *args, size_t argc,
                   float *out);

int smc_call_int(smc_expr_id_t expr_id,
                 const int64_t *args, size_t argc,
                 int64_t *out);

/* -------------------------------------------------------------------------- */
/* Variables                                                                  */
/* -------------------------------------------------------------------------- */

/* Bind a variable in the global context before evaluation. */
int smc_set_variable_double(const char *name, double value);
int smc_set_variable_double_with(smc_context_t *ctx, const char *name, double value);

/* Clear all variable bindings. */
int smc_clear_variables(void);
int smc_clear_variables_with(smc_context_t *ctx);

/* -------------------------------------------------------------------------- */
/* Cache control                                                              */
/* -------------------------------------------------------------------------- */

int smc_cache_clear(void);
int smc_cache_clear_with(smc_context_t *ctx);

int smc_cache_save(const char *path);
int smc_cache_save_with(smc_context_t *ctx, const char *path);

int smc_cache_load(const char *path);
int smc_cache_load_with(smc_context_t *ctx, const char *path);

/* -------------------------------------------------------------------------- */
/* Source generation (build-time optimizer output)                            */
/* -------------------------------------------------------------------------- */

/* Generate a C source file containing hot cached expressions. */
int smc_generate_c_source(const char *out_path);
int smc_generate_c_source_with(smc_context_t *ctx, const char *out_path);

/* -------------------------------------------------------------------------- */
/* Expression metadata (Tier 2)                                               */
/* -------------------------------------------------------------------------- */

/* Return the number of expressions available in the generated dispatch table. */
int smc_expr_count(void);

/* Return the arity (number of free variables) of expression ID. */
size_t smc_expr_arity(smc_expr_id_t id);

/* Return the original expression string for expression ID. */
const char *smc_expr_source(smc_expr_id_t id);

/* -------------------------------------------------------------------------- */
/* Error handling                                                             */
/* -------------------------------------------------------------------------- */

/* Return a human-readable string for an error code. */
const char *smc_error_string(int code);

/* Return the last error recorded in the global context. */
const smc_error_t *smc_last_error(void);

/* Return the last error recorded in an explicit context. */
const smc_error_t *smc_last_error_with(smc_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* SMC_H */
```

### 3.2 Design Notes

- **ABI stability**: All public types use fixed-width integers or opaque pointers. No Lisp objects, no structs with public layout.
- **Thread awareness**: `smc_context_t*` instances are isolated. The global context uses internal locking; explicit contexts are not shared across threads unless externally synchronized.
- **No strings on hot path**: `smc_call_*` uses stable integer IDs. Generated headers may also expose named wrappers (e.g., `smc_expr_lighting_dot_sin(...)`), but these are implementation details of the generated file, not part of the stable ABI.
- **Error model**: Functions return `0` on success and a non-zero error code on failure. Detailed errors are available via `smc_last_error*`.

---

## 4. Idiomatic Python API

### 4.1 Module (`python/smc/__init__.py`)

```python
import smc

# Tier 1: global context, expression strings
result = smc.eval("2 + 3 * 4")          # returns float
result = smc.eval_int("5 // 2")        # returns int

# Variables
smc.set_variable("x", 3.0)
result = smc.eval("x^2 + 5*x + 6")

# Tier 1: explicit context
ctx = smc.Context(level=2)
ctx.set_variable("x", 3.0)
result = ctx.eval("x^2 + 5*x + 6")

# Tier 2: production generated-code calls
result = smc.call(42, 0.6, 0.0, 0.8)   # smc_call_double by expr_id

# Cache persistence
smc.cache_save("my_cache.smc")
smc.cache_load("my_cache.smc")

# Build-time source generation
smc.generate_c_source("smc_generated.c")
```

### 4.2 Design Notes

- **Zero build step**: Uses `ctypes` to load `libsmc.so` / `libsmc.dylib` / `libsmc.dll` at import time.
- **Natural types**: `smc.eval()` returns `float`, `smc.eval_int()` returns `int`. Future vector support may return tuples or NumPy arrays.
- **Context manager support**: `with smc.Context(level=3) as ctx:` is supported for deterministic cleanup.
- **Future `compile()`**: Documented but not implemented in v1. It will return a callable backed by `smc_call_*` once Tier 2 is available.

---

## 5. Integration Strategy Comparison

| Approach | Pros | Cons | Recommended For |
|----------|------|------|-----------------|
| **Generated C code** | No runtime Lisp; deterministic; ABI stable; easy to ship; fastest warm path | Requires build step; only optimizes seen expressions | **Production games, simulations, deployment builds** |
| **Embedded SBCL runtime** | Full runtime self-modification; rapid iteration; arbitrary expressions | Large binary; GC pauses; harder to ship; thread model constraints | **Development, prototyping, tools** |
| **Static library** | Simple linking; no runtime dependency beyond libc | No dynamic updates; larger executables | C/C++ game engines |
| **Shared library** | Easy updates; multiple languages bind | ABI must be frozen; versioning burden | Python packages, plugin systems |
| **IPC/service model** | Language agnostic; process isolation | Latency; serialization overhead; operational complexity | Microservices, remote compute |
| **FFI into live SBCL** | Maximum flexibility | Complex; not ABI stable; threading pitfalls | Research, not production |
| **Generated code (hot paths)** | Best warm performance; smallest runtime | Requires trace/warmup data | **Renderers, physics loops** |

### 5.1 Recommended Game/Simulation Architecture

1. **Offline training phase**: Run representative workloads in SBCL with `smc:configure-optimization(3)`. SMC populates its cache and rewrites source.
2. **Build phase**: Run `scripts/generate-c-source.lisp` to emit `smc_generated.c` containing hot-path dispatch tables. Compile `smc_generated.c` + `smc_runtime.c` into a static or shared library.
3. **Runtime**: Game calls `smc_call_double(expr_id, args, argc, &out)` for hot math. Known expressions hit the generated dispatch; unknown expressions fall back to the Tier 1 evaluator (if SBCL is embedded) or a small interpreter.
4. **Optional live SBCL runtime**: During development, link against the SBCL-backed runtime so new expressions can be evaluated without a full rebuild.

---

## 6. Milestones

### Milestone 1 — C API & Python Binding

**Goal**: A stable, usable C API and Python package, backed by either a standalone stub runtime or an optional SBCL runtime.

**Deliverables**:

1. `include/smc.h` — stable C ABI v1 (Tier 1).
2. `src/c/smc_runtime_stub.c` — standalone C runtime for Tier 1 (no SBCL dependency).
3. `src/c/smc_runtime_sbcl.c` — optional SBCL-backed runtime for full fidelity.
4. `scripts/generate-c-source.lisp` — initial C emitter (may emit a simple dispatch table).
5. `python/smc/__init__.py` — `ctypes` binding for Tier 1.
6. `examples/c/hello_smc.c` — builds and runs with `gcc`.
7. `examples/python/hello_smc.py` — runs without a build step.
8. Acceptance tests:
   - C test passes for `smc_eval_double` and variable binding.
   - Python test passes for `smc.eval` and `smc.Context`.
   - Generated C source compiles and produces identical results to the SBCL runtime.

**Acceptance Criteria**:

- `gcc examples/c/hello_smc.c -lsmc -o hello_smc && ./hello_smc` prints correct results.
- `python examples/python/hello_smc.py` prints correct results.
- All existing Lisp tests still pass.

### Milestone 2 — Production Generated-Code API

**Goal**: The generated C path becomes the true production hot path with stable expression IDs.

**Deliverables**:

1. Extend `include/smc.h` with Tier 2 (`smc_call_*`, expression IDs, metadata).
2. Full C code generator in Lisp: walk the SMC cache, assign stable IDs to hot expressions, emit `switch`/function dispatch in C.
3. `src/c/smc_generated_runtime.c` — small runtime linking generated dispatch tables with Tier 1 fallback.
4. `examples/c/renderer_hotpath.c` — demonstrates `smc_call_double(SMC_EXPR_DOT_LIGHT, args, 3, &out)`.
5. Python binding updated to expose `smc.call(expr_id, *args)`.
6. Acceptance test: generated code runs without SBCL and matches SBCL results.

**Acceptance Criteria**:

- A program compiled with only `smc_generated.c` + `smc_runtime.c` evaluates cached expressions correctly with no SBCL present.
- `smc_call_double` is measurably faster than `smc_eval_double` on warm expressions.

### Milestone 3 — Game/Simulation Integration

**Goal**: SMC is packaged and documented for integration into real projects.

**Deliverables**:

1. CMake package (`CMakeLists.txt`) with static/shared library options.
2. `setup.py` / `pyproject.toml` for `pip install smc`.
3. Integration guide (`docs/integration-guide.md`).
4. Benchmark suite comparing:
   - Generated C code
   - SBCL runtime
   - Conventional evaluation
5. Example renderer hotpath in C and Python.

**Acceptance Criteria**:

- A user can `pip install -e python/` and run `import smc`.
- A user can `cmake -B build && cmake --build build` and link `libsmc.a` or `libsmc.so`.
- Benchmarks show the generated C path meeting or exceeding the 5× warm speedup target on realistic renderer workloads.

---

## 7. Runtime Source Rewriting in Production

Runtime source rewriting remains part of the architecture, but its role changes:

- **In SBCL development runtime**: `configure-optimization(3)` continues to rewrite `cache/generated/cache-literals.lisp` to warm up the cache across sessions.
- **In generated C builds**: The C code generator reads the warmed cache and emits equivalent C literals/dispatch tables. The generated file is the "rewritten" artifact for deployment.
- **Future**: A build-time tool can run SBCL headlessly, warm the cache on a trace corpus, and emit `smc_generated.c` as part of CI.

This preserves the self-modifying philosophy while producing a conventional, auditable C artifact for shipping.

---

## 8. Threading Model

- **Global context**: Serialized by a mutex. Safe to call from multiple threads, but concurrent calls are serialized.
- **Explicit contexts**: Each `smc_context_t*` is independent. Threads should use their own contexts for parallel work.
- **Generated C code**: Stateless and lock-free. Multiple threads may call `smc_call_*` concurrently.
- **SBCL runtime**: SBCL has a stop-the-world GC. Long-lived embedded use in a threaded game should prefer generated C for hot paths and isolate SBCL to a background optimization thread.

---

## 9. Risks & Mitigations

| Risk | Mitigation |
|------|------------|
| Generated C code diverges from SBCL semantics | Acceptance tests compare every generated expression against SBCL output |
| ABI breaks in future versions | Versioned API (`SMC_ABI_VERSION`), opaque pointers, fixed-width types |
| SBCL runtime too heavy for games | Generated C path has no SBCL dependency; SBCL is optional |
| Python `ctypes` overhead on hot path | Tier 2 `smc.call` uses stable IDs; future cffi/CPython extension optional |
| Cache warmup data not representative | Provide tooling to record and replay trace corpora |

---

## 10. Milestone 1 Status

| Deliverable | Status | Notes |
|-------------|--------|-------|
| `include/smc.h` | ✅ Complete | Stable ABI v1 with error codes and public `smc_error_t` layout |
| `src/c/smc_runtime_stub.c` | ✅ Complete | Standalone C runtime with arithmetic parser/evaluator |
| `src/c/smc_runtime_sbcl.c` | ✅ Placeholder | Documented placeholder; full SBCL wiring deferred |
| `python/smc/__init__.py` | ✅ Complete | Pure-Python `ctypes` binding |
| `scripts/generate-c-source.lisp` | ✅ Complete | Emits generated C dispatch table |
| `examples/c/hello_smc.c` | ✅ Complete | Builds and runs |
| `examples/python/hello_smc.py` | ✅ Complete | Runs without build step |
| Acceptance tests | ✅ Complete | `tests/c/test_stub_runtime.c` and `tests/python/test_smc.py` pass |

### How to build and run Milestone 1

**C shared library:**
```bash
gcc -std=c99 -Wall -Wextra -fPIC -Iinclude -shared src/c/smc_runtime_stub.c -o libsmc.so -lm
```

**C example:**
```bash
gcc -std=c99 -Wall -Wextra -Iinclude src/c/smc_runtime_stub.c examples/c/hello_smc.c -o hello_smc -lm
./hello_smc
```

**C acceptance test:**
```bash
gcc -std=c99 -Wall -Wextra -Iinclude src/c/smc_runtime_stub.c tests/c/test_stub_runtime.c -o test_stub_runtime -lm
./test_stub_runtime
```

**Python example:**
```bash
PYTHONPATH=python python3 examples/python/hello_smc.py
```

**Python acceptance test:**
```bash
PYTHONPATH=python python3 tests/python/test_smc.py
```

**Generate C source from SBCL:**
```bash
sbcl --script scripts/generate-c-source.lisp /tmp/smc_generated.c
```

### Known limitations of Milestone 1

- The stub runtime only supports scalar arithmetic (`+`, `-`, `*`, `/`, `^`), parentheses, and unary `+`/`-`.
- Variables, cache persistence, source generation, and Tier 2 `smc_call_*` return `SMC_ERR_NOT_IMPL` in the stub runtime.
- The SBCL-backed runtime is a placeholder; wiring it requires Lisp-side `sb-alien` callback registration and an image build step.
- The generator script emits a small hard-coded dispatch table as a proof of concept. Milestone 2 will walk the real SMC cache.

## 11. Milestone 2 Status

| Deliverable | Status | Notes |
|-------------|--------|-------|
| `include/smc.h` Tier 2 | ✅ Complete | `smc_call_*`, `smc_expr_id_t`, expression metadata |
| `scripts/generate-c-source.lisp` | ✅ Complete | Walks the real SMC cache, assigns stable IDs, emits C dispatch table |
| `src/c/smc_generated_runtime.c` | ✅ Complete | Weak fallback Tier 2 stubs; overridden by generated `smc_generated.c` |
| `examples/c/renderer_hotpath.c` | ✅ Complete | Demonstrates `smc_call_double` in a tight loop |
| Python `smc.call` + metadata | ✅ Complete | `smc.call`, `smc.expr_count`, `smc.expr_arity`, `smc.expr_source` |
| Acceptance tests | ✅ Complete | `tests/c/test_generated.c` and `tests/python/test_smc.py` pass against generated code |

### How to build and run Milestone 2

**Generate C source from the SMC cache:**
```bash
sbcl --script scripts/generate-c-source.lisp build/smc_generated.c
```

**C shared library with generated hot path:**
```bash
gcc -std=c99 -Wall -Wextra -fPIC -Iinclude -shared \
    src/c/smc_runtime_stub.c src/c/smc_generated_runtime.c build/smc_generated.c \
    -o build/libsmc.so -lm
```

**C generated-code acceptance test:**
```bash
gcc -std=c99 -Wall -Wextra -Iinclude \
    src/c/smc_runtime_stub.c src/c/smc_generated_runtime.c build/smc_generated.c \
    tests/c/test_generated.c -o build/test_generated -lm
./build/test_generated
```

**Python generated-code acceptance test:**
```bash
PYTHONPATH=python python3 tests/python/test_smc.py
```

### Known limitations of Milestone 2

- The generator currently emits ground (variable-free) scalar expressions only.
- Free-variable expressions require arity tracking and argument substitution in generated C.
- The generated macro names are verbose; a future revision may use short hashes.

## 12. Next Step

Begin **Milestone 3** implementation:

1. Add a `CMakeLists.txt` with static/shared library options.
2. Add `python/pyproject.toml` for `pip install -e python/`.
3. Write `docs/integration-guide.md` with game/simulation examples.
4. Add a benchmark suite comparing generated C, SBCL runtime, and conventional evaluation.
5. Provide a Python renderer hot-path example.

---

*This document is the currently active and prioritized plan for SMC C/Python embedding. Milestones 1 and 2 are complete.*
