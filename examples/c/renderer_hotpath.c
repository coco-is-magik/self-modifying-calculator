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
 * expression. In a real engine this would be per-pixel or per-vertex. */
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

    /* Warm up and then time the hot loop. */
    const int iterations = 1000000;
    volatile double accumulator = 0.0;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 1; i <= iterations; i++) {
        /* Cycle through the generated expression IDs. */
        smc_expr_id_t id = (smc_expr_id_t)((i % count) + 1);
        double t = (double)i * 0.001;
        accumulator += compute_intensity(id, t);
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
