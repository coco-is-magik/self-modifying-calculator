/* tests/benchmarks/benchmark_dispatch_overhead.c
 * Compare smc_call_double vs inline C for a simple expression.
 *
 * Build with CMake (SMC_BUILD_BENCHMARKS) or manually:
 *   gcc -std=c99 -O2 -Iinclude \
 *       src/c/smc_runtime_stub.c src/c/smc_generated_runtime.c build/smc_generated.c \
 *       tests/benchmarks/benchmark_dispatch_overhead.c -o build/benchmark_dispatch_overhead -lm
 */

#define _POSIX_C_SOURCE 200809L

#include "smc.h"

#include <math.h>
#include <stdio.h>
#include <time.h>

/* A simple expression: (2 + 3) * 4  — same as what the generator produces for "(2+3)*4" */
static double inline_eval(void) {
    return (2.0 + 3.0) * 4.0;
}

int main(void) {
    if (smc_init() != SMC_OK) {
        fprintf(stderr, "smc_init failed\n");
        return 1;
    }

    int count = smc_expr_count();
    if (count == 0) {
        printf("No generated dispatch table; run the generator first.\n");
        smc_shutdown();
        return 0;
    }

    /* Find the ground (arity=0) expression with the simplest arithmetic.
     * We'll use it to compare dispatch overhead vs inline C. */
    smc_expr_id_t target_id = 1;
    for (int id = 1; id <= count; id++) {
        if (smc_expr_arity((smc_expr_id_t)id) == 0) {
            target_id = (smc_expr_id_t)id;
            break;
        }
    }

    const int iterations = 10000000; /* 10M iterations */
    struct timespec start, end;
    double elapsed_smc, elapsed_inline;

    /* Benchmark smc_call_double */
    {
        volatile double accumulator = 0.0;
        clock_gettime(CLOCK_MONOTONIC, &start);
        for (int i = 0; i < iterations; i++) {
            double out = 0.0;
            (void)smc_call_double(target_id, NULL, 0, &out);
            accumulator += out;
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        elapsed_smc = (end.tv_sec - start.tv_sec) +
                      (end.tv_nsec - start.tv_nsec) / 1e9;
        (void)accumulator;
    }

    /* Benchmark inline C */
    {
        volatile double accumulator = 0.0;
        clock_gettime(CLOCK_MONOTONIC, &start);
        for (int i = 0; i < iterations; i++) {
            accumulator += inline_eval();
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        elapsed_inline = (end.tv_sec - start.tv_sec) +
                         (end.tv_nsec - start.tv_nsec) / 1e9;
        (void)accumulator;
    }

    printf("=== Dispatch Overhead Benchmark ===\n");
    printf("Expression: id=%d source=\"%s\"\n",
           target_id, smc_expr_source(target_id));
    printf("Iterations: %d\n", iterations);
    printf("smc_call_double: %.6f s  (%.0f calls/sec)\n",
           elapsed_smc, iterations / elapsed_smc);
    printf("inline C:        %.6f s  (%.0f calls/sec)\n",
           elapsed_inline, iterations / elapsed_inline);
    printf("Overhead ratio (smc / inline): %.2fx\n",
           elapsed_smc / elapsed_inline);
    printf("Overhead per call: %.1f ns\n",
           (elapsed_smc - elapsed_inline) / iterations * 1e9);

    smc_shutdown();
    return 0;
}