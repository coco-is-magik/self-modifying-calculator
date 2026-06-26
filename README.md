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

*(More sophisticated syntax with operator precedence, variables, and function support coming in future phases.)*

---

## Roadmap

The project is organized into five implementation phases:

### Phase 1 — Foundation ✅
- [x] Set up SBCL + ASDF project structure
- [x] AST representation and parser (string → AST)
- [x] Basic evaluator (AST → result)
- [x] CLI entry point

### Phase 2 — Caching Engine ✅
- [x] In-memory hierarchical cache (whole + sub-expressions)
- [x] Expression matcher (tree isomorphism for partial matching)
- [x] Persistent cache (serialized to disk)
- [x] Cache-aware evaluator

### Phase 3 — Self-Modification ✅
- [x] Level 1: Runtime hash cache
- [x] Level 2: Runtime function specialization + hot-swapping
- [x] Level 3: Source-level rewriting for persistent optimization

### Phase 4 — Performance Optimization & Benchmarking (In Progress)
- [ ] Compiled cache dispatch table to replace EQUAL hash lookup
- [ ] Vector / linear algebra module (`:vec3`, `:dot`, `:cross`, `:norm`, `:normalize`)
- [ ] Refactor benchmark suite into per-category benchmarks
- [ ] Measure speedup per category: arithmetic, dot product, cross product, trig, polynomial, mixed, realistic renderer
- [ ] Track cache hit ratios, cold vs warm performance per optimization level

### Phase 5 — Math Modules (Planned)
- [ ] Algebra (quadratics, factoring, polynomials)
- [ ] Trigonometry (sine/cosine operators for benchmarks)
- [ ] Calculus (numerical derivatives, integrals)
- [ ] Statistics

### Phase 6 — Polish (Planned)
- [ ] Full documentation and examples
- [ ] Integration tests
- [ ] Benchmarks and demos

> **Full architectural plan**: See [`docs/plan.md`](docs/plan.md)

## Performance Direction

Initial benchmarks show that the cache-aware evaluator is **not yet faster than the conventional evaluator for simple arithmetic** and only marginally faster for dot products. The root cause is that EQUAL hash-table lookup on AST lists is more expensive than the arithmetic operations it skips.

The current corrective plan is:
1. **Compiled cache dispatch table**: generate a compiled `cond`/`case` function for whole-expression cache hits, eliminating hash computation and list traversal.
2. **Vector/linear algebra module**: add realistic renderer operations (dot/cross products, vector normalization) where the cache saves more expensive work.
3. **Per-category benchmarks**: split the benchmark suite by math domain so strengths and weaknesses are visible.

See `docs/plan.md` for the full performance analysis and `docs/HANDOFF.md` for the detailed implementation plan.

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
  --eval "(load \"tests/test-runner.lisp\")" \
  --eval "(load \"tests/core/test-ast.lisp\")" \
  --eval "(load \"tests/core/test-parser.lisp\")" \
  --eval "(load \"tests/core/test-evaluator.lisp\")" \
  --eval "(smc:run-all-tests)" \
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
│   └── plan.md              # Full architectural plan (source of truth)
├── src/
│   ├── core/                 # Engine: AST, cache, evaluator, optimizer
│   ├── math/                 # Pluggable math modules
│   ├── interface/            # CLI and parser
│   └── main.lisp             # Entry point
├── tests/                    # Test suites
├── cache/                    # Persistent cache data
├── calculator.lsp            # Current CLISP calculator (backward compat)
├── README.md
└── LICENSE
```

---

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.