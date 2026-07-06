/* smc_generated_runtime.c — weak fallback runtime for generated C code */
/*
 * This file provides weak fallback implementations for the Tier 2 generated-code
 * API. When a generated dispatch table (smc_generated.c) is linked, its strong
 * definitions override these fallbacks. When no generated table is present,
 * these fallbacks ensure the library still links and returns SMC_ERR_NOT_IMPL
 * for Tier 2 calls.
 *
 * Link this file together with smc_runtime_stub.c for the Milestone 1 runtime,
 * or with smc_runtime_stub.c + smc_generated.c for the Milestone 2 runtime.
 */

#include "smc.h"

#include <stddef.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/* Tier 2 generated-code calls                                                */
/* -------------------------------------------------------------------------- */

/* Global statistics shared with the runtime.  The generated dispatch table
    increments these counters directly.
    Defined in smc_runtime_stub.c; declared extern here so both files can
    reference the same object without a duplicate-symbol error at link time. */
extern smc_stats_t smc_global_stats;

/* Fallback dispatch when no generated table is linked. The generated table
 * overrides these with strong definitions. We distinguish:
 *   - SMC_ERR_NOT_IMPL: no generated table linked (expr_count == 0)
 *   - SMC_ERR_NOT_FOUND: generated table linked but ID unknown
 *   - SMC_ERR_ARITY: generated table linked but wrong argc
 *   - SMC_ERR_INVALID: null output pointer
 */
static int smc_fallback_call_double(smc_expr_id_t expr_id,
                                    const double *args, size_t argc,
                                    double *out) {
    (void)expr_id;
    (void)args;
    (void)argc;
    smc_global_stats.total_calls++;
    if (out == NULL) {
        smc_global_stats.invalid_calls++;
        return SMC_ERR_INVALID;
    }
    smc_global_stats.fallback_evals++;
    return SMC_ERR_NOT_IMPL;
}

__attribute__((weak)) int smc_call_double(smc_expr_id_t expr_id,
                                          const double *args, size_t argc,
                                          double *out) {
    return smc_fallback_call_double(expr_id, args, argc, out);
}

__attribute__((weak)) int smc_call_float(smc_expr_id_t expr_id,
                                         const float *args, size_t argc,
                                         float *out) {
    (void)expr_id;
    (void)args;
    (void)argc;
    if (out == NULL) {
        smc_global_stats.invalid_calls++;
        return SMC_ERR_INVALID;
    }
    smc_global_stats.total_calls++;
    smc_global_stats.fallback_evals++;
    return SMC_ERR_NOT_IMPL;
}

__attribute__((weak)) int smc_call_int(smc_expr_id_t expr_id,
                                        const int64_t *args, size_t argc,
                                        int64_t *out) {
    (void)expr_id;
    (void)args;
    (void)argc;
    if (out == NULL) {
        smc_global_stats.invalid_calls++;
        return SMC_ERR_INVALID;
    }
    smc_global_stats.total_calls++;
    smc_global_stats.fallback_evals++;
    return SMC_ERR_NOT_IMPL;
}

/* -------------------------------------------------------------------------- */
/* Expression metadata                                                        */
/* -------------------------------------------------------------------------- */

__attribute__((weak)) int smc_expr_count(void) {
    return 0;
}

__attribute__((weak)) size_t smc_expr_arity(smc_expr_id_t id) {
    (void)id;
    return 0;
}

__attribute__((weak)) const char *smc_expr_source(smc_expr_id_t id) {
    (void)id;
    return NULL;
}

/* Runtime ABI-version check. A generated table overrides this. */
__attribute__((weak)) int smc_generated_abi_version(void) {
    return 0;
}
