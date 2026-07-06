/* examples/c/renderer_hotpath.c — example game/simulation hot path */
/*
 * This example shows how a renderer or simulation loop can use SMC's Tier 2
 * generated-code API to evaluate hot cached expressions by stable ID. The
 * generated dispatch table is produced ahead of time by the SMC build-time
 * optimizer (scripts/generate-c-source.lisp) and linked into the final
 * binary, so the hot loop has no SBCL dependency and no string parsing.
 *
 * Build:
 *   sbcl --script scripts/generate-c-source.lisp build/smc_generated.c
 *   gcc -Iinclude -o examples/c/renderer_hotpath \
 *       examples/c/renderer_hotpath.c \
 *       src/c/smc_runtime_stub.c src/c/smc_generated_runtime.c \
 *       build/smc_generated.c -lm
 */

#define _POSIX_C_SOURCE 200809L

#include "smc.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* A pretend renderer frame: compute a color intensity from a generated
 * expression. In a real engine this would be per-pixel or per-vertex.
 *
 * This example uses a generated expression that takes one argument (t).
 * The default generated dispatch table includes "x ^ 2" (arity 1), so
 * we use that ID and pass t as the single argument. */
static double compute_intensity(smc_expr_id_t id, double t) {
    double out = 1.0;
    int rc = smc_call_double(id, &t, 1, &out);
    if (rc != SMC_OK) {
        /* Fall back to a safe constant on error. */
        out = 1.0;
    }
    return out;
}

int main(void) {
    if (smc_init() != SMC_OK) {
        fprintf(stderr, "smc_init failed\n");
        return 1;
    }

    int count = smc_expr_count();
    printf("Renderer hot path ready. %d generated expression(s) available.\n", count);

    if (count == 0) {
        printf("No generated expressions; nothing to benchmark.\n");
        smc_shutdown();
        return 0;
    }

    /* Find a generated expression with arity 1 so we can pass t as a
     * real argument. The default generated table includes "x ^ 2". */
    smc_expr_id_t hot_id = 0;
    for (int id = 1; id <= count; id++) {
        if (smc_expr_arity((smc_expr_id_t)id) == 1) {
            hot_id = (smc_expr_id_t)id;
            break;
        }
    }
    if (hot_id == 0) {
        printf("No arity-1 generated expression found; nothing to benchmark.\n");
        smc_shutdown();
        return 0;
    }

    const char *hot_source = smc_expr_source(hot_id);
    printf("Hot expression id=%u source=%s\n", (unsigned int)hot_id,
           hot_source ? hot_source : "(null)");

    /* Warm up and then time the hot loop. */
    const int iterations = 1000000;
    volatile double accumulator = 0.0;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 1; i <= iterations; i++) {
        double t = (double)i * 0.001;
        accumulator += compute_intensity(hot_id, t);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("Computed %d hot-path calls in %.3f s (%.0f calls/s)\n",
           iterations, elapsed, iterations / elapsed);
    printf("Accumulator (prevent DCE): %g\n", accumulator);

    smc_shutdown();
    return 0;
}
