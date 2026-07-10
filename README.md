# Self-Modifying Calculator

A command-line calculator written in Common Lisp that **optimizes itself for repeated, similar calculations** by caching not just whole expressions but their sub-parts. Each time it performs an operation, it rewrites itself — at runtime and/or at the source level — to refer to previously computed results instead of recalculating.

> **Target use cases**: Repeated mathematical operations in renderers, 3D games, physics simulations, and any scenario where similar computations happen many times.

---

## Table of Contents
- [How It Works](#how-it-works)
- [Examples](#examples)
- [Roadmap](#roadmap)
- [Installation](#installation)
- [Usage](#usage)
- [Project Structure](#project-structure)
- [License](#license)

---

## How It Works

Expressions are represented as ASTs (Abstract Syntax Trees). The system caches **both whole expressions and their sub-expressions**, enabling partial reuse. Three levels of self-modification provide increasing optimization depth:

| Level | Mechanism | Persistence |
|-------|-----------|-------------|
| **1** | Runtime hash table cache (O(1) lookup) | Per-session |
| **2** | Runtime function redefinition via `fdefinition` | Per-session |
| **3** | Source file rewriting with embedded constants | Across sessions |

### Example: Dot Product

```
N⋅L = (0)(0.6) + (0)(0) + (1)(0.8)

Sub-expressions cached:
(0 × 0.6) → 0
(0 × 0)   → 0
(1 × 0.8) → 0.8

If N changes slightly to (0, 0, 2):
Only (2 × 0.8) → 1.6 needs computation
The rest hits cache → no recalculation needed
```

### Example: Quadratic Equation

```
x² + 5x + 6 = 0  factors to  (x + 2)(x + 3) = 0

Sub-expressions cached:
(x + 2) → individual factor result
(x + 3) → individual factor result
(x + 2)(x + 3) → combined factor result

If a later problem uses (x + 2) as a sub-expression,
the cached result is reused directly.
```

---

## Examples

### Basic Arithmetic
```bash
./scripts/run-calculator.sh "5+7"
# 12

./scripts/run-calculator.sh "8-4"
# 4

./scripts/run-calculator.sh "4*2"
# 8

./scripts/run-calculator.sh "10/5"
# 2
```

### Function Call Syntax

The parser now supports function-call syntax for trig, vector, and statistics functions:

```bash
sbcl --noinform \
  --eval "(pushnew *default-pathname-defaults* asdf:*central-registry*)" \
  --eval "(asdf:load-system :self-modifying-calculator)" \
  --eval "(format t \"~A~%\" (smc:run-calculator \"sin(1.57079632679)\"))" \
  --eval "(sb-ext:exit)"
# 1.0

sbcl --noinform \
  --eval "(pushnew *default-pathname-defaults* asdf:*central-registry*)" \
  --eval "(asdf:load-system :self-modifying-calculator)" \
  --eval "(format t \"~A~%\" (smc:run-calculator \"dot(vec3(1,0,0), vec3(0,1,0))\"))" \
  --eval "(sb-ext:exit)"
# 0.0
```

### Unified Optimization Pipeline

Enable all three levels of self-modification with one call:

```lisp
(smc:configure-optimization 3)  ; Level 1 cache + Level 2 specialization + Level 3 source rewriting
```

- Level 1: runtime cache (always active)
- Level 2: operator specialization via `fdefinition`
- Level 3: source rewriting to `cache/generated/cache-literals.lisp` + automatic persistence across sessions

### Automatic Cache Persistence

Set `smc:*auto-persist-cache*` to `t` (or use `configure-optimization 3`) and the global cache is loaded on startup and saved on exit:

```bash
sbcl --noinform \
  --eval "(pushnew *default-pathname-defaults* asdf:*central-registry*)" \
  --eval "(asdf:load-system :self-modifying-calculator)" \
  --eval "(setf smc:*auto-persist-cache* t)" \
  --eval "(smc:main '(\"2+3\"))" \
  --eval "(sb-ext:exit)"
```

---

## Roadmap

The project is organized into implementation phases:

### Phase 1 — Foundation ✅
- [x] Set up SBCL + ASDF project structure (no Quicklisp)
- [x] AST representation and parser (string → AST)
- [x] Basic evaluator (AST → result)
- [x] CLI entry point (`main.lisp` + `run.sh`)

### Phase 2 — Caching Engine ✅
- [x] In-memory hierarchical cache (whole + sub-expressions)
- [x] Expression matcher (tree isomorphism for partial matching)
- [x] Persistent cache (serialized to disk)
- [x] Cache-aware evaluator

### Phase 3 — Self-Modification ✅
- [x] Level 1: Runtime hash cache
- [x] Level 2: Runtime function specialization + hot-swapping
- [x] Level 3: Source-level rewriting for persistent optimization

### Phase 4 — Performance Optimization & Benchmarking ✅
- [x] Canonical AST interning + EQ hash table (replaced EQUAL hash lookup)
- [x] Vector / linear algebra module (`:vec3`, `:dot`, `:cross`, `:norm`, `:normalize`, `:vec3-add`, `:mat4x4-mul`)
- [x] Trigonometry module (`:sin`, `:cos`)
- [x] Refactor benchmark suite into per-category benchmarks
- [x] Measure speedup per category: arithmetic, dot product, cross product, trig, polynomial, mixed, realistic renderer, matrix multiply, Blinn-Phong, end-to-end parse+eval
- [x] Track cache hit ratios, cold vs warm performance
- [x] Multi-trial aggregation with median + range
- [x] Isolated optimization-level benchmarks (baseline, L1, L1.5, L2)

### Phase 5 — Math Modules ✅
- [x] Arithmetic (`+`, `-`, `*`, `/`, `^`)
- [x] Algebra (quadratics, polynomial evaluation)
- [x] Calculus (numerical derivatives, integrals)
- [x] Linear algebra (`:vec3`, `:dot`, `:cross`, `:norm`, `:normalize`)
- [x] Trigonometry (`:sin`, `:cos`)
- [x] Statistics (mean, variance, std-dev, median, sum, min, max)

### Phase 6 — Polish (In Progress)
- [ ] Full documentation and examples
- [x] Integration tests
- [x] Cache eviction / size bounding (LRU policy added)
- [x] Benchmarks and demos (per-category benchmark suite exists)
- [x] Revisit compiled cache dispatch with a smarter data structure (limited to 100 clauses; falls back to EQ hash table)
- [x] Unify the 3-level optimization into an automatic pipeline (`configure-optimization`)
- [x] Function call syntax in the string parser (`sin(x)`, `dot(a,b)`, `vec3(x,y,z)`)
- [x] Automatic cache persistence on startup/shutdown
- [x] Parser cache eviction with configurable max size

> **Known issues and limitations**: See [`docs/KNOWN-ISSUES.md`](docs/KNOWN-ISSUES.md)


## C & Python Embedding (Milestone 1 Complete; Milestones 2–3 In Progress)

SMC has a working embeddable scaffold from **C** and **Python**, but the production path is not fully mature. The embedding layer is staged across three milestones:

| Milestone | Status | Description |
|-----------|--------|-------------|
| **Milestone 1** | ✅ Complete | Stable C ABI v1, standalone stub runtime, Python `ctypes` binding, and a C generator |
| **Milestone 2** | ✅ Complete | Production generated-code API (`smc_call_*`) works for ground and argumentized expressions; the default build flow emits arity-1 and arity-2 expressions |
| **Milestone 3** | 🔄 In Progress | CMake package, `pyproject.toml`, integration guide, and game/simulation benchmarks exist; remaining work is MSan verification, a broad generated-code cross-check harness, and macOS/Windows install testing |

> **Note**: The C API maturity plan in [`docs/c-api-maturity-plan.md`](docs/c-api-maturity-plan.md) tracks the remaining work. The most important current gaps are: verifying MSan with Clang, extending the generated-code cross-check harness, and documenting macOS/Windows build steps.

### Architecture

- **Primary production target**: Generate C source code from SMC's hot cached expressions. The runtime is plain C with no Lisp dependency.
- **Development / tooling target**: An optional embedded SBCL runtime for full self-modification during prototyping.
- **Python binding**: Pure-Python `ctypes` wrapper with zero build step.

### C API v1

The stable header is `include/smc.h`. The formal ABI contract is in `docs/abi-contract.md`. Two API tiers are exposed:

- **Tier 1 — Development/tooling** (`smc_eval_double`, `smc_eval_float`, `smc_eval_int`): evaluate arbitrary expression strings. Not recommended for frame-budgeted hot loops.
- **Tier 2 — Production hot path** (`smc_call_double`, `smc_call_float`, `smc_call_int`): call cached expressions by stable integer ID with no string parsing.

Tier 2 is implemented by a generated dispatch table (`smc_generated.c`) produced by the build-time optimizer. The generated table overrides weak fallback stubs in `src/c/smc_generated_runtime.c`.

### Python API

```python
import smc

# Tier 1: global context, expression strings
print(smc.eval("2 + 3 * 4"))        # 14.0
print(smc.eval_int("7 / 2"))        # 3

# Tier 1: isolated context
with smc.Context(level=2) as ctx:
    print(ctx.eval("10 - 4 / 2"))   # 8.0

# Tier 2: production generated-code call by stable ID
print(smc.expr_count())             # number of generated expressions
print(smc.expr_source(1))           # original expression string for id 1
print(smc.call(1))                  # evaluate generated expression id 1
```

### Building and Running

**CMake (recommended):**
```bash
# 1. Generate C source from the SMC cache
sbcl --script scripts/generate-c-source.lisp build/smc_generated.c

# 2. Configure and build
cmake -B build -S . -DSMC_GENERATED_SOURCE=build/smc_generated.c
cmake --build build

# 3. Run C tests
ctest --test-dir build --output-on-failure
```

**CMake options:**

| Option | Default | Description |
|--------|---------|-------------|
| `SMC_BUILD_SHARED` | ON | Build `libsmc.so` |
| `SMC_BUILD_STATIC` | ON | Build `libsmc.a` and `libsmc_generated.a` |
| `SMC_BUILD_EXAMPLES` | ON | Build `hello_smc` and `renderer_hotpath` |
| `SMC_BUILD_TESTS` | ON | Build C acceptance tests |
| `SMC_BUILD_BENCHMARKS` | ON | Build C benchmarks including `benchmark_indexed_state` |
| `SMC_GENERATED_SOURCE` | "" | Path to `smc_generated.c` |

**Python install from source:**
```bash
pip install -e python/
export LD_LIBRARY_PATH=$PWD/build:$LD_LIBRARY_PATH
python3 -c "import smc; print(smc.call(1))"
```

**Python examples:**
```bash
PYTHONPATH=python LD_LIBRARY_PATH=build python3 examples/python/hello_smc.py
PYTHONPATH=python LD_LIBRARY_PATH=build python3 examples/python/renderer_hotpath.py
```

**Python acceptance test:**
```bash
PYTHONPATH=python LD_LIBRARY_PATH=build python3 tests/python/test_smc.py
```

**Regression tests:**
- `ctest --test-dir build --output-on-failure` runs C acceptance tests, including:
  - `test_symbol_visibility` — verifies the stable C ABI symbols are exported from `libsmc`.
  - `test_pedantic_compile` — verifies `include/smc.h` and the C runtime files compile with `-Wall -Wextra -Werror -pedantic -std=c99`.
- `./scripts/run-tests.sh` runs Lisp tests, including:
  - Shell-script syntax checks for every `scripts/*.sh`.
  - A check that `scripts/generate-c-source.lisp` uses only exported `smc:` symbols (no `smc::` internal access).
- `PYTHONPATH=python LD_LIBRARY_PATH=build python3 tests/python/test_smc.py` runs Python tests, including:
  - Importability of every public name and every `ctypes` type used by the binding.
  - Successful module load when a generated dispatch table is present.

**Benchmarks:**
```bash
# C benchmarks
./build/benchmark_generated
./build/benchmark_indexed_state  # Dense state tracking API benchmark

# Python
PYTHONPATH=python LD_LIBRARY_PATH=build python3 tests/benchmarks/benchmark_embedding.py
```

### Indexed-State API (v2.1)

For dense array data where each element has a stable integer index (renderers, ECS, tile maps, particle systems), use the indexed state API. This avoids hashing overhead and provides significantly faster performance than the generic state API.

```c
// Configuration
smc_state_indexed_config_t config = {
    .count = 41600,          // Number of indexed slots (e.g., 260x160 grid)
    .state_size = 8,         // Size of each state record
    .memory_budget_bytes = 0   // Auto-computed
};
smc_state_indexed_configure(ctx, &config);

// Scalar check (fast path for single indices)
int changed = 0;
smc_state_changed_index(ctx, index, &state, sizeof(state), &changed);

// Batch check (reduces per-call overhead for large batches)
size_t dirty_count = 0;
smc_state_diff_indexed_batch(ctx, states, count, stride, dirty_indices, capacity, &dirty_count);
```

Typical performance for 41,600 records with 8-byte state structs:
- Generic unchanged: ~250-320 ns/op
- Indexed unchanged: ~30 ns/op (8-10x faster)
- Batch unchanged: ~15-40 ns/op (6-20x faster)
- Generic changed: ~600-2100 ns/op
- Indexed changed: ~40 ns/op (15x faster)
- Batch changed: ~15-60 ns/op (10-40x faster)

### Limitations