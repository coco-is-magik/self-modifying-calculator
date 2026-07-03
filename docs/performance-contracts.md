# SMC Performance Contracts

> **Scope**: When the Self-Modifying Calculator (SMC) generated C path wins, when it loses, and what game/simulation workloads should expect.

---

## 1. Workloads Where SMC Wins

SMC is designed for **high reuse** of the same mathematical expression with different scalar inputs.

| Workload | Why It Wins |
|----------|-------------|
| Renderer shading | Same `dot(light, normal)`, `pow`, `attenuation` expressions evaluated per pixel/vertex with different inputs. |
| Animation curves | Same easing or IK expression evaluated for many bones/particles. |
| Physics/materials | Repeated scalar force, energy, or damping calculations. |
| Procedural generation | Same noise/combination expressions sampled at many positions. |

The current benchmark suite reports **~11× warm speedups** on renderer-style workloads with **>99.8% cache hit rates**.

---

## 2. Workloads Where SMC Loses

| Workload | Why It Loses |
|----------|--------------|
| One-off expressions | `smc_eval_double` parses a string every call. Overhead dominates. |
| Mostly unique expressions | Cache hit rate drops; generated table grows without reuse. |
| Heavy vector/matrix math | ABI v1 is scalar-only. Vector work requires domain adapters or future ABI revision. |
| Tight loops already hand-optimized | A generated expression is rarely faster than a manually inlined C expression. |

---

## 3. Performance Tiers

| Tier | Latency | Use Case |
|------|---------|----------|
| Tier 2 generated (`smc_call_*`) | ~1 function call + switch + arithmetic | Hot loops, per-pixel/per-vertex work |
| Tier 1 string eval (`smc_eval_*`) | Parse + evaluate | Tooling, prototyping, one-off fallback |
| Embedded SBCL runtime | Parse + Lisp eval + possible GC | Development, research, runtime specialization |

---

## 4. Reproducibility Requirements

For stable performance in production:

1. **Warm the cache on a representative corpus** before generating C source.
2. **Use stable expression IDs**; do not rely on ID ordering across generator runs.
3. **Link the generated table correctly**: static link order is `smc_generated` **after** `smc`.
4. **Keep the generated table immutable** at runtime for lock-free Tier 2 calls.

---

## 5. Benchmarks

The repository includes:

- `tests/benchmarks/benchmark_generated.c` — C comparison of Tier 1 vs Tier 2.
- `tests/benchmarks/benchmark_embedding.py` — Python comparison of pure Python, Tier 1, and Tier 2.

Run the C benchmark after generating a dispatch table:

```bash
sbcl --script scripts/generate-c-source.lisp build/smc_generated.c
cmake -B build -S . -DSMC_GENERATED_SOURCE=build/smc_generated.c
cmake --build build
./build/benchmark_generated
```

---

## 6. Game Integration Guidance

1. **Offline**: profile your game to find hot math expressions.
2. **Train**: run those expressions through `smc:run-calculator` or `smc:configure-optimization` in SBCL.
3. **Generate**: emit `smc_generated.c` with `scripts/generate-c-source.lisp`.
4. **Build**: link `smc_generated` + `smc` into your game.
5. **Runtime**: call `smc_call_double(expr_id, args, argc, &out)` in hot loops.
6. **Monitor**: use `smc_get_stats()` to detect unexpected fallbacks.

---

## 7. Related Documents

- `docs/abi-contract.md` — formal ABI contract.
- `docs/integration-guide.md` — C and Python integration steps.
- `docs/fallback-model.md` — production fallback behavior.
