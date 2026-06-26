# Self-Modifying Calculator — Implementation Plan

> **Status**: Draft Plan
> **Last Updated**: 2026-06-26

This document is the source of truth for the Self-Modifying Calculator project. It describes the architecture, module structure, key design decisions, and phased implementation roadmap.

---

## Table of Contents

- [Vision](#vision)
- [Architectural Foundation](#architectural-foundation)
- [Core Concept — Hierarchical Expression Caching](#core-concept--hierarchical-expression-caching)
- [Module Structure](#module-structure)
- [Key Design Decisions](#key-design-decisions)
- [Self-Modification Mechanism](#self-modification-mechanism)
- [Expression Matching Algorithm](#expression-matching-algorithm)
- [Implementation Phases](#implementation-phases)

---

## Vision

A calculator that optimizes itself for repeated, similar calculations by caching not just whole expressions but their sub-parts. Each time it performs an operation, it rewrites itself (at runtime and/or at the source level) to refer to previously computed results instead of recalculating.

**Target use cases:**
- Repeated mathematical operations in renderers and 3D games
- Quadratic equations where sub-expressions (factors) can be reused
- Dot products and vector math where individual component products repeat
- Any scenario where similar computations happen many times

---

## Architectural Foundation

**Language**: Common Lisp — target **SBCL** (Steel Bank Common Lisp) for the production runtime.

SBCL advantages over the current CLISP runtime:
- Native compilation with aggressive optimization
- `compile` for generating optimized functions at runtime
- Superior macro system for code rewriting
- Better performance for compute-heavy use cases (renderer/game math)

**Build System**: ASDF (Another System Definition Facility) with Quicklisp for dependency management.

---

## Core Concept — Hierarchical Expression Caching

Every expression is represented as an **AST (Abstract Syntax Tree)**. The system caches both whole expressions AND their sub-expressions, enabling **partial reuse**.

```
Example: N⋅L = (0)(0.6)+(0)(0)+(1)(0.8)

AST Representation:
(:+ (:* 0 0.6) (:* 0 0) (:* 1 0.8))

Cached Results:
(:* 0 0.6) → 0
(:* 0 0)   → 0
(:* 1 0.8) → 0.8
(:+ 0 0 0.8) → 0.8
(:+ (:* 0 0.6) (:* 0 0) (:* 1 0.8)) → 0.8

If later computing N⋅L with N=(0,0,2):
Only (:* 1 0.8) changes to (:* 2 0.8) → 1.6
The rest hits cache → no recomputation needed.
```

### Example: Quadratic Equation

```
Expression: x^2 + 5x + 6 = 0

Factored form: (x+2)(x+3) = 0

Sub-expression breakdown:
(:* (:+ x 2) (:+ x 3)) → factors result
(:+ x 2) → single factor result
(:+ x 3) → single factor result

Cached:
(:+ 2 2) would be cached from elsewhere → 4
If another problem uses (:+ x 2), it reuses cached result.
```

---

## Module Structure

```
self-modifying-calculator/
├── src/
│   ├── core/                      # Engine
│   │   ├── package.lisp           # Package definitions
│   │   ├── ast.lisp               # Expression AST representation
│   │   ├── matcher.lisp           # Pattern matching & sub-expression detection
│   │   ├── cache.lisp             # In-memory + persistent caching layer
│   │   ├── optimizer.lisp         # Code generation & self-modification
│   │   └── evaluator.lisp         # Expression evaluator w/ cache injection
│   │
│   ├── math/                      # Math modules (pluggable)
│   │   ├── arithmetic.lisp        # +, -, *, /, modulo, powers
│   │   ├── algebra.lisp           # Quadratics, factoring, polynomial ops
│   │   ├── linear-algebra.lisp    # Vectors, dot/cross products, matrices
│   │   ├── trigonometry.lisp      # sin, cos, tan, their inverses
│   │   ├── calculus.lisp          # Numerical derivatives, integrals
│   │   └── statistics.lisp        # Mean, variance, etc.
│   │
│   ├── interface/                 # User interaction
│   │   ├── cli.lisp               # Command-line interface
│   │   └── parser.lisp            # String → AST parser
│   │
│   └── main.lisp                  # Entry point
│
├── cache/                         # Persistent cache directory
│   └── cache.sexp                 # Serialized cache data
│
├── tests/
│   ├── core/
│   │   ├── test-ast.lisp
│   │   ├── test-cache.lisp
│   │   └── test-evaluator.lisp
│   ├── math/
│   │   ├── test-arithmetic.lisp
│   │   └── test-algebra.lisp
│   └── integration/
│       └── test-integration.lisp
│
├── calculator.lsp                 # Backward-compatible wrapper
├── README.md
├── LICENSE
└── self-modifying-calculator.asd  # ASDF system definition
```

---

## Key Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| **Expression representation** | S-expression AST `(:op args...)` | Lisp-native, easy to match, serialize, and rewrite |
| **Cache key** | Full AST equality + structural isomorphism for sub-expressions | Enables partial reuse of sub-computations |
| **Cache storage** | In-memory hash table + serialized `.sexp` file on disk | Fast runtime access + persistence across sessions |
| **Self-modification mechanism** | `defun` redefinition + `compile` at runtime + source file rewriting | Three levels of optimization from light to deep |
| **Expression parser** | Pratt parser for operator precedence | Handles complex expressions, extensible to new operators |
| **Variable support** | Symbolic binding in evaluation context | Needed for equations like `x^2 + 5x + 6` |
| **Testing framework** | FiveAM or Prove | Well-established Lisp testing libraries |
| **CLI format** | Natural mathematical syntax with precedence (`2*x^2 + 5*x + 6`) | More intuitive and capable than the current `5+7` format |

---

## Self-Modification Mechanism

The system modifies itself at three levels, each more aggressive than the last:

### Level 1 — Runtime Caching (fastest, per-session)

```lisp
;; After computing (+ 2 2) once:
(cached-lookup '(:+ 2 2)) → 4  ;; O(1) hash lookup

;; Sub-parts are also cached:
(cached-lookup '(:+ (: * 2 2) (:* 3 5))) → 19  ;; if both sub-expressions cached
```

**Mechanism**: Hash table lookup before evaluation. The evaluator checks the cache for the current AST node and all sub-nodes. Only uncached sub-trees are evaluated.

### Level 2 — Function Specialization (medium, persists within session)

```lisp
;; Generate a specialized function that short-circuits known inputs:
(defun +-specialized (a b)
  (cond ((and (= a 2) (= b 2)) 4)
        ((and (= a 5) (= b 3)) 8)
        (t (+ a b))))

;; Redefine the ADD function at runtime:
(setf (fdefinition 'add) #'+-specialized)
```

**Mechanism**: After enough cache hits on a particular operator with particular operands, the optimizer generates a specialized version of that operator function and hot-swaps it via `fdefinition`.

### Level 3 — Source Code Rewriting (persistent across sessions)

```lisp
;; After many runs, the system modifies calculator.lsp itself
;; to insert cached lookup tables as literal constants,
;; so they compile directly into the binary on next load.
```

**Mechanism**: The optimizer serializes the accumulated cache into Lisp source code that gets written back to the calculator source files. On the next load, these values are compiled in as literals, providing immediate optimization without a warm-up period.

---

## Expression Matching Algorithm

For sub-expression caching, the system uses a **tree isomorphism** check:

1. **Decompose**: Given a new expression AST, decompose it into all sub-trees.
2. **Look Up**: Check each sub-tree against the cache.
3. **Replace**: Substitute matched sub-trees with their cached values.
4. **Evaluate**: Compute only the unmatched (new) portions.

```
Input: (:+ (:* 2 3) (:* 4 5))
Cache: (:* 2 3) → 6
       (:* 4 5) → 20
Result: Replace both products → (:+ 6 20) → cache lookup → 26
        No actual multiplication performed at runtime.
```

For partial matches:

```
Input: (:* (:+ 2 3) (:+ 4 5))
Cache: (:+ 2 3) → 5
No cache for (:+ 4 5) or (:* 5 ...)
Result: Replace only the known sub-expression:
        (:* 5 (:+ 4 5)) → only the (:+ 4 5) needs actual evaluation
```

---


## Performance Benchmarking Suite

A dedicated benchmarking framework to measure the self-modifying calculator's performance against conventional (non-caching, non-self-modifying) calculation. The benchmark suite generates randomized calculation series at three deterministic lengths, times both approaches, and reports comparative metrics.

### Benchmark Design

#### Conventional Baseline
- Standard Common Lisp arithmetic — no caching, no self-modification, no AST lookup
- Fresh evaluation of every expression each time
- Represents the "naive" approach that the self-modifying calculator aims to beat

#### Self-Modifying Variant
- Same calculation series, processed by the self-modifying calculator
- Warm-up period: the first pass builds the cache via Level 1 (hash table)
- Subsequent passes benefit from cache hits (Level 1), function specialization (Level 2), and source-level optimization (Level 3)

#### Series Lengths (Deterministic, Randomized Content)

| Series | Number of Calculations | Purpose |
|--------|----------------------|---------|
| **Short** | 100 | Measure overhead vs benefit for small workloads |
| **Medium** | 10,000 | Realistic batch size for a typical computation loop |
| **Long** | 1,000,000 | Stress test for the caching and optimization pipeline |

Each series is generated with deterministic seeding for reproducibility. The expressions within each series are randomized across a constrained set of patterns to simulate repeated similar calculations:

- **Arithmetic series**: Random operands (bounded range), random operators from `(+-*/)`
- **Vector series**: Random 2D/3D vectors, repeated dot product and cross product calculations
- **Polynomial series**: Random coefficients, evaluation at repeated points
- **Mixed series**: Combinations of the above to test sub-expression reuse across different problem types

### Benchmark Execution

```
For each series length (short / medium / long):
  1. Generate the randomized expression series (seed-controlled)
  2. Run the conventional baseline: evaluate all expressions serially, timing total wall-clock
  3. Reset state
  4. Run the self-modifying variant:
     a. First pass: evaluate all expressions (populates cache)
     b. Second pass: evaluate all expressions again (hits cache)
     c. Third pass: evaluate all expressions again (triggers function specialization)
     d. (If Level 3 available) Serialize cache, reload, evaluate again
  5. Record and compare: total time, per-expression average time, cache hit ratio
```

### Metrics Collected

| Metric | Description |
|--------|-------------|
| **Total wall-clock time** | Elapsed real time from start to finish |
| **Per-expression average time** | Total time ÷ number of expressions |
| **Speedup factor** | Conventional time ÷ self-modifying time (per category) |
| **Cache hit ratio** | Number of cache hits ÷ total AST nodes evaluated |
| **Optimization level reached** | Which levels (1/2/3) engaged during the run |
| **Cold vs warm performance** | First pass time vs repeated pass time |

### Explicit Performance Goals

The self-modifying calculator **must beat conventional calculation** across all series categories. The target thresholds:

| Category | Minimum Speedup Factor | Stretch Goal |
|----------|----------------------|--------------|
| **Short series (100)** | 1.0× (no slower than conventional) | 1.5× |
| **Medium series (10,000)** | 2.0× | 5.0× |
| **Long series (1,000,000)** | 5.0× | 20.0×+ |

The primary focus is achieving maximum speedup on the **long series**, where the caching and self-modification overhead is amortized over the largest number of repeated sub-expressions. The short series exists primarily to ensure the overhead of cache lookups does not make simple calculations slower.

### Benchmarking File

A dedicated benchmarking module lives at:

```
tests/benchmarks/
├── benchmark-runner.lisp     # Orchestrator: series generation, timing, reporting
├── series-generator.lisp     # Randomized expression series (deterministic seed)
├── conventional-eval.lisp    # Naive evaluation baseline
├── smc-eval.lisp             # Self-modifying calculator evaluation harness
├── metric-report.lisp        # Comparison reporting and visualization
└── results/                  # Directory for benchmark output files
```

This benchmarking suite is executed as its own step during development (not part of the standard test suite) and results are tracked over time to monitor optimization progress.

---

## Implementation Phases

### Phase 1 — Foundation
- [ `] Set up SBCL + ASDF + Quicklisp project structure
- [ ] Create ASDF system definition (`self-modifying-calculator.asd`)
- [ ] Implement AST representation (`core/ast.lisp`)
- [ ] Implement Pratt parser (string → AST) (`interface/parser.lisp`)
- [ ] Implement basic evaluator (AST → result) (`core/evaluator.lisp`)
- [ ] Port CLI to SBCL (`interface/cli.lisp`)
- [ ] Create main entry point (`main.lisp`)
- [ ] Write core test suite (`tests/core/`)

### Phase 2 — Caching Engine
- [ ] Implement in-memory cache (hash table) (`core/cache.lisp`)
- [ ] Implement hierarchical caching (whole + sub-expression)
- [ ] Implement expression matcher (tree isomorphism) (`core/matcher.lisp`)
- [ ] Implement persistent cache (read/write `.sexp` files) (`core/cache.lisp`)
- [ ] Implement cache-aware evaluator (check cache before computing)
- [ ] Test with simple arithmetic sequences

### Phase 3 — Self-Modification
- [ ] Implement Level 1 (runtime hash cache) — verify speedup
- [ ] Implement Level 2 (runtime function redefinition via `fdefinition`) (`core/optimizer.lisp`)
- [ ] Implement compiled cache lookup table generation
- [ ] Implement Level 3 (source-level rewriting for persistence)
- [ ] Implement the 3-level optimization pipeline
- [ ] Benchmark: compare cached vs uncached evaluation
- [ ] Test with dot product and quadratic examples

### Phase 4 — Math Modules
- [ ] Implement pluggable math module registration system
- [ ] Arithmetic module: enhance basic ops (`+`, `-`, `*`, `/`) with caching support
- [ ] Algebra module: quadratics, factoring, polynomial operations
- [ ] Linear algebra module: vectors, dot product, cross product, matrix operations
- [ ] Trigonometry module: `sin`, `cos`, `tan`, inverses
- [ ] Calculus module: numerical derivatives, integrals
- [ ] Register all modules in the evaluator dispatch

### Phase 5 — Polish & Documentation
- [ ] Update README with new architecture and syntax
- [ ] Add comprehensive usage examples (quadratic, dot product, repeated operations)
- [ ] Fill in LICENSE with proper copyright holder
- [ ] Add inline documentation and docstrings
- [ ] Create demo scripts showing speed improvement
- [ ] Write integration tests for full workflows

---

## Performance Analysis and Corrective Plan

### Why Simple Arithmetic Is Currently Slower

Benchmarking revealed that the cache-aware evaluator is **slower than the conventional evaluator for simple arithmetic** and only marginally faster for dot products. The root cause is that the cache lookup cost exceeds the cost of the operations it skips.

For a single expression like `2+3`:
- **Conventional path**: tag check, extract two constants, apply `+` (one CPU instruction).
- **SMC path**: hash the AST list `(:+ 2 3)` with `EQUAL`, probe the hash table, increment a counter, then return the cached value.

The hash lookup is more expensive than the addition. In the arithmetic benchmark, roughly 37% of the 100,000 expressions are unique, so the evaluator pays the lookup cost on every evaluation and only gets a hit on the other 63%.

### Why Dot Products Are Slightly Faster

The dot-product benchmark computes `(nx*0.6)+(ny*0.0)+(nz*0.8)` with `nx, ny, nz ∈ {-1, 0, 1}`. There are only 27 unique whole expressions and 9 unique products. After the first pass, nearly every sub-expression and every whole expression is cached. The warm pass shows ~1.20× speedup because the saved work (3 multiplications + 2 additions) finally exceeds the lookup cost, but the `EQUAL` hash lookup still consumes a significant portion of the runtime.

### Corrective Strategy

The selected path forward is **A + D**:

1. **Compiled cache dispatch table** (Option A): Replace the generic `EQUAL` hash table with a compiled `cond`/`case` function for whole-expression cache hits. This eliminates hash computation and list traversal for warm expressions.
2. **Vector/linear algebra module** (Option D): Add `:vec3`, `:dot`, `:cross`, `:norm`, and `:normalize` operators. This provides a realistic renderer workload where the cache saves more expensive work.

### Additional Benchmark Refactoring

The benchmark suite will be split by math domain so we can measure and communicate per-category performance:

- **Arithmetic**: scalar `+ - * / ^` with small operands (hardest case for cache overhead)
- **Dot product**: `dot(vec3, vec3)` with fixed light direction (high sub-expression reuse)
- **Cross product**: `cross(vec3, vec3)` with random vectors (moderately expensive)
- **Trigonometry**: `sin`/`cos` combinations (expensive library calls, high cache value)
- **Polynomial**: quadratic evaluation with repeated coefficients (factor reuse)
- **Mixed**: random mixture of categories
- **Realistic renderer**: combined lighting, dot product, and trig ops

Each category runs at short (100), medium (10,000), and long (100,000) lengths. The summary table reports conventional time, SMC cold time, SMC warm time, and speedup.

---

## Open Questions (To Be Resolved During Development)

1. **SBCL vs CLISP backward compatibility**: Should we maintain a CLISP-compatible code path, or fully commit to SBCL?
2. **Level 3 depth**: True source-code rewriting or just cache persistence in a data file?
3. **Expression syntax**: Support for variables (`x`, `y`), functions (`sin(x)`), and how they interact with caching.
4. **Cache eviction policy**: How to bound cache size — LRU, TTL, or unlimited?
5. **Thread safety**: Is concurrency a requirement for the target use case (renderers are often threaded)?
6. **Dispatch recompilation strategy**: Recompile lazily on cache miss, or compile once when the cache reaches a threshold?
7. **Vector representation**: Use plain 3-element lists for cache-key compatibility, or switch to arrays for faster access?

---

*This document is the source of truth for the project. Update it as the architecture evolves.*
