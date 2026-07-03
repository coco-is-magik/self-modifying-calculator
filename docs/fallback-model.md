# SMC Production Fallback Model

> **Scope**: How a C host program should handle errors from `smc_call_*` and when it is safe to fall back to Tier 1 evaluation.

---

## 1. Error Taxonomy

The Tier 2 hot path (`smc_call_double`, `smc_call_float`, `smc_call_int`) can return the following error codes:

| Code | Meaning | Recommended Host Action |
|------|---------|------------------------|
| `SMC_OK` (0) | Success | Use the result. |
| `SMC_ERR_NOT_FOUND` | Unknown expression ID | The ID is not in the generated table. If the runtime has Tier 1 support, evaluate the source string. Otherwise use a hard-coded safe value. |
| `SMC_ERR_ARITY` | Wrong number of arguments | Caller bug. Fix the call site. Do not fall back to Tier 1 with the same arity. |
| `SMC_ERR_INVALID` | Null output pointer or invalid args | Caller bug. Fix the call site. |
| `SMC_ERR_NOT_IMPL` | No generated table linked and no Tier 1 fallback | Use a hard-coded safe value or abort the hot path. |
| `SMC_ERR_ABI` | Generated table ABI mismatch | Do not call Tier 2. Rebuild the generated table against the current runtime header. |
| `SMC_ERR_INIT` | Library not initialized | Call `smc_init()` once per process before any Tier 2 calls. |

---

## 2. Decision Tree

```
smc_call_double(id, args, argc, &out)
        |
        v
    SMC_OK? ----> use out
        |
        no
        |
    SMC_ERR_NOT_FOUND?
        |
        yes ---> do you have the source expression?
                    |
                    yes ---> smc_eval_double(source, &out)   (Tier 1)
                    |
                    no  ---> use safe default
        |
        no
        |
    SMC_ERR_ARITY / SMC_ERR_INVALID?
        |
        yes ---> log bug, use safe default, fix call site
        |
        no
        |
    SMC_ERR_NOT_IMPL / SMC_ERR_ABI / SMC_ERR_INIT?
        |
        yes ---> disable hot path, use safe default, fix build/init
```

---

## 3. Tier 1 Is Tooling-Only

`smc_eval_*` parses a string on every call. It is explicitly **not recommended** for frame-budgeted hot loops. A production renderer or simulation should:

1. Warm the cache offline in SBCL.
2. Generate C source with `scripts/generate-c-source.lisp`.
3. Call by stable ID in the hot loop.
4. Only fall back to `smc_eval_*` for expressions that were not in the training corpus, and only if the frame budget allows.

---

## 4. Safe Defaults

A host program should choose a safe default value that does not crash the simulation or produce NaN/Inf artifacts:

- Color/intensity: `1.0` (full white / neutral).
- Position/offset: `0.0`.
- Boolean predicates: `0.0` (false).

The `examples/c/renderer_hotpath.c` example demonstrates this pattern:

```c
static double compute_intensity(smc_expr_id_t id, double t) {
    double out = 1.0;
    int rc = smc_call_double(id, &t, 1, &out);
    if (rc != SMC_OK) {
        out = 1.0;  /* safe default */
    }
    return out;
}
```

---

## 5. Observability

Use `smc_get_stats()` to monitor fallback behavior in production:

- `fallback_evals` rising: many Tier 2 calls are missing from the generated table.
- `invalid_ids` rising: the host is calling with stale or wrong IDs.
- `arity_errors` rising: a call site passes the wrong argument count.

---

## 6. Related Documents

- `docs/abi-contract.md` — formal ABI contract.
- `docs/integration-guide.md` — C and Python integration steps.
- `docs/performance-contracts.md` — when SMC wins and when overhead dominates.
