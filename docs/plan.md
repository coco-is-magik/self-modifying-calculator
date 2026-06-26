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

## Open Questions (To Be Resolved During Development)

1. **SBCL vs CLISP backward compatibility**: Should we maintain a CLISP-compatible code path, or fully commit to SBCL?
2. **Level 3 depth**: True source-code rewriting or just cache persistence in a data file?
3. **Expression syntax**: Support for variables (`x`, `y`), functions (`sin(x)`), and how they interact with caching.
4. **Cache eviction policy**: How to bound cache size — LRU, TTL, or unlimited?
5. **Thread safety**: Is concurrency a requirement for the target use case (renderers are often threaded)?

---

*This document is the source of truth for the project. Update it as the architecture evolves.*