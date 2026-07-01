# SMC Integration Guide

This guide explains how to integrate the Self-Modifying Calculator (SMC) into C and Python projects, with a focus on game and simulation workloads.

---

## Table of Contents

- [Overview](#overview)
- [C Integration](#c-integration)
  - [Build with CMake](#build-with-cmake)
  - [Use the Generated Hot Path](#use-the-generated-hot-path)
  - [Linking Notes](#linking-notes)
- [Python Integration](#python-integration)
  - [Install from Source](#install-from-source)
  - [Use the Python API](#use-the-python-api)
- [Recommended Workflow](#recommended-workflow)
- [Threading](#threading)
- [Troubleshooting](#troubleshooting)

---

## Overview

SMC has two integration modes:

1. **Generated C code (recommended for production)** — compile hot cached expressions into plain C. No SBCL runtime is required. Fast, deterministic, and ABI-stable.
2. **Embedded SBCL runtime (development / tooling)** — full runtime self-modification. Larger and harder to ship; useful for prototyping.

This guide focuses on the generated C path, which is the production target.

---

## C Integration

### Build with CMake

The repository includes a `CMakeLists.txt` that builds:

- `libsmc.so` / `libsmc.a` — the runtime (Tier 1 + weak Tier 2 fallbacks)
- `libsmc_generated.so` / `libsmc_generated.a` — the generated dispatch table (strong Tier 2 implementations)
- Examples and tests

```bash
# 1. Generate C source from the SMC cache
sbcl --script scripts/generate-c-source.lisp build/smc_generated.c

# 2. Configure and build
cmake -B build -S . -DSMC_GENERATED_SOURCE=build/smc_generated.c
cmake --build build

# 3. Run tests
ctest --test-dir build --output-on-failure
```

Build options:

| Option | Default | Description |
|--------|---------|-------------|
| `SMC_BUILD_SHARED` | ON | Build `libsmc.so` / `libsmc_generated.so` |
| `SMC_BUILD_STATIC` | ON | Build `libsmc.a` / `libsmc_generated.a` |
| `SMC_BUILD_EXAMPLES` | ON | Build `hello_smc` and `renderer_hotpath` |
| `SMC_BUILD_TESTS` | ON | Build C acceptance tests |
| `SMC_GENERATED_SOURCE` | "" | Path to `smc_generated.c` |

### Use the Generated Hot Path

```c
#include "smc.h"
#include <stdio.h>

int main(void) {
    smc_init();

    int count = smc_expr_count();
    printf("Hot expressions available: %d\n", count);

    for (int id = 1; id <= count; id++) {
        double out = 1.0;
        int rc = smc_call_double((smc_expr_id_t)id, NULL, 0, &out);
        if (rc == SMC_OK) {
            printf("id=%d %s = %g\n", id, smc_expr_source((smc_expr_id_t)id), out);
        }
    }

    smc_shutdown();
    return 0;
}
```

Link against `smc` and `smc_generated` (order matters for static linking: generated after runtime):

```bash
gcc myapp.c -Lbuild -lsmc_generated -lsmc -lm -o myapp
```

### Linking Notes

- **Shared libraries**: link `smc_generated` then `smc`. The dynamic linker resolves strong generated symbols correctly.
- **Static libraries**: link `smc_generated` **after** `smc`. The generated object files must be seen after the weak fallback stubs so the strong definitions override them.
- **No generated table**: if you omit `SMC_GENERATED_SOURCE`, Tier 2 calls return `SMC_ERR_NOT_IMPL` and `smc_expr_count()` returns 0.

---

## Python Integration

### Install from Source

The Python binding is pure `ctypes` and has no build step, but it needs a compiled `libsmc.so` (or `libsmc.dylib` / `libsmc.dll`) at runtime.

```bash
# 1. Build the C shared library with generated hot path
sbcl --script scripts/generate-c-source.lisp build/smc_generated.c
cmake -B build -S . -DSMC_GENERATED_SOURCE=build/smc_generated.c
cmake --build build

# 2. Install the Python package
pip install -e python/

# 3. Make sure libsmc.so and libsmc_generated.so are findable
export LD_LIBRARY_PATH=$PWD/build:$LD_LIBRARY_PATH
```

The package looks for the shared library next to the repository root first, then in the system linker path.

### Use the Python API

```python
import smc

# Tier 1: evaluate expression strings
print(smc.eval("2 + 3 * 4"))      # 14.0
print(smc.eval_int("7 / 2"))      # 3

# Tier 1: isolated context
with smc.Context(level=2) as ctx:
    print(ctx.eval("10 - 4 / 2"))  # 8.0

# Tier 2: generated hot path
print(smc.expr_count())            # number of generated expressions
for expr_id in range(1, smc.expr_count() + 1):
    source = smc.expr_source(expr_id)
    value = smc.call(expr_id)
    print(f"{source} = {value}")
```

---

## Recommended Workflow

For a game or simulation:

1. **Profile** your workload to identify hot math expressions.
2. **Warm the cache** in SBCL by running representative inputs through `smc:run-calculator` or `smc:configure-optimization`.
3. **Generate C source** with `scripts/generate-c-source.lisp`.
4. **Build** your project with the generated dispatch table linked.
5. **Call** `smc_call_double(expr_id, args, argc, &out)` in your hot loop.
6. **Iterate**: when expressions change, re-run the generator and rebuild.

---

## Threading

- `smc_call_*` is stateless and lock-free. Multiple threads may call it concurrently.
- The global context (`smc_eval_double`, etc.) is internally serialized.
- Use explicit `smc_context_t*` instances per thread for parallel Tier 1 evaluation.

---

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| `smc_expr_count()` returns 0 | No generated table linked | Pass `SMC_GENERATED_SOURCE` to CMake or link `smc_generated` |
| `smc_call_double` returns `SMC_ERR_NOT_IMPL` | Generated table not overriding weak stubs | For static linking, put `smc_generated` after `smc` on the link line |
| Python `ImportError: libsmc.so` | Shared library not found | Set `LD_LIBRARY_PATH` or copy `.so` next to the package |
| Generated IDs differ between builds | Cache contents changed | IDs are stable for a fixed cache; document the warm-up corpus |
