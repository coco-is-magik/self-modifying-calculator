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
clisp calculator.lsp 5+7
# 12

clisp calculator.lsp 8-4
# 4

clisp calculator.lsp 4*2
# 8

clisp calculator.lsp 10/5
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
- [x] Vector / linear algebra module (`:vec3`, `:dot`, `:cross`, `:norm`, `:normalize`)
- [x] Trigonometry module (`:sin`, `:cos`)
- [x] Refactor benchmark suite into per-category benchmarks
- [x] Measure speedup per category: arithmetic, dot product, cross product, trig, polynomial, mixed, realistic renderer
- [x] Track cache hit ratios, cold vs warm performance

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


## Performance Results

The cache-aware evaluator now outperforms conventional evaluation on realistic workloads. Key optimizations:
1. **Canonical AST interning + EQ hash table**: identical expressions share the same object, so cache lookups use fast pointer comparison instead of structural `EQUAL`.
2. **Removed explicit AST rewriting**: the evaluator no longer builds an intermediate rewritten AST on every cache miss.
3. **Vector/trigonometry modules**: realistic operations (dot products, cross products, trig) give the cache more expensive work to skip.

### LONG series speedups (100,000 calculations)

| Category | Speedup |
|---|---|
| Arithmetic | **1.17×** |
| Dot Product | **3.33×** |
| Cross Product | **3.00×** |
| Trig | **1.20×** |
| Polynomial | **3.50×** |
| Mixed | **2.67×** |
| Realistic Renderer | **3.25×** |

Arithmetic remains the hardest category because the operations themselves are so cheap. All renderer-style workloads (dot, cross, polynomial, realistic renderer) show solid 3×+ speedups.

Short and medium series are measured by repeating each pass 100× and 10× respectively, then dividing by the repetition count; long series are the most reliable source of speedup numbers.

See `docs/plan.md`, `docs/HANDOFF.md`, and `docs/notes/session-4-implementation-notes.md` for the full analysis and implementation details.

---

## Installation

### Prerequisites
- **SBCL** (Steel Bank Common Lisp) — required runtime
- **CLISP** — optional, for backward compatibility with the existing `calculator.lsp`

SBCL is required because the project relies on native compilation and runtime code generation (`compile`, `fdefinition`) to implement self-modification.

### Setup
```bash
git clone https://github.com/coco-is-magik/self-modifying-calculator.git
cd self-modifying-calculator
```

## Usage

### SBCL (new implementation)
```bash
# Using the run.sh wrapper:
./run.sh "2+3*4"

# Or invoke directly with ASDF:
cd /bigdisk/programming/self-modifying-calculator
sbcl --noinform \
  --eval "(pushnew *default-pathname-defaults* asdf:*central-registry*)" \
  --eval "(asdf:load-system :self-modifying-calculator)" \
  --eval "(smc:main '(\"2+3*4\"))" \
  --eval "(sb-ext:exit)"

# Run the test suite:
sbcl --noinform \
  --eval "(pushnew *default-pathname-defaults* asdf:*central-registry*)" \
  --eval "(asdf:load-system :self-modifying-calculator)" \
  --eval "(load \"tests/load-all-tests.lisp\")" \
  --eval "(sb-ext:exit)"

# Run the quick demo:
sbcl --noinform \
  --eval "(pushnew *default-pathname-defaults* asdf:*central-registry*)" \
  --eval "(asdf:load-system :self-modifying-calculator)" \
  --eval "(load \"demo.lisp\")" \
  --eval "(sb-ext:exit)"

# Run the full benchmark suite (long-running):
sbcl --noinform \
  --eval "(pushnew *default-pathname-defaults* asdf:*central-registry*)" \
  --eval "(asdf:load-system :self-modifying-calculator)" \
  --eval "(load \"tests/benchmarks/benchmark-framework.lisp\")" \
  --eval "(load \"tests/benchmarks/series-generators.lisp\")" \
  --eval "(load \"tests/benchmarks/run-all-benchmarks.lisp\")" \
  --eval "(smc:run-all-benchmarks)" \
  --eval "(sb-ext:exit)"
```

### CLISP (legacy backward-compatible calculator)
```bash
clisp calculator.lsp 5+7
```

---

## Project Structure

```
self-modifying-calculator/
├── docs/
│   ├── plan.md              # Full architectural plan (source of truth)
│   ├── HANDOFF.md           # Session-by-session development log
│   ├── CHANGELOG.md         # Change history (created)
│   ├── KNOWN-ISSUES.md      # Open gaps and technical debt
│   └── notes/               # Detailed implementation notes
├── src/
│   ├── core/                 # Engine: AST, cache, evaluator, optimizer
│   ├── math/                 # Pluggable math modules
│   ├── interface/            # CLI and parser
│   └── main.lisp             # Entry point
├── tests/                    # Test suites
├── cache/                    # Persistent cache data
├── demo.lisp                 # Quick demo script
├── calculator.lsp            # Current CLISP calculator (backward compat)
├── README.md
└── LICENSE
```

---

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.