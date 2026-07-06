/* tests/benchmarks/benchmark_throughput.c
 * Measure throughput (calls/sec) per expression ID for all generated expressions.
 *
 * Build with CMake (SMC_BUILD_BENCHMARKS) or manually:
 *   gcc -std=c99 -O2 -Iinclude \
 *       src/c/smc_runtime_stub.c src/c/smc_generated_runtime.c build/smc_generated.c \
 *       tests/benchmarks/benchmark_throughput.c -o build/benchmark_throughput -lm
 */

#define _POSIX_C_SOURCE 200809L

#include "smc.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

    const int iterations = 1000000; /* 1M calls per expression */
    struct timespec start, end;

    printf("=== Throughput Benchmark ===\n");
    printf("iterations=%d\n\n", iterations);
    printf("%-4s %-35s %-6s %12s %12s\n",
           "ID", "Expression", "Arity", "Time (s)", "Calls/sec");

    for (int id = 1; id <= count; id++) {
        const char *source = smc_expr_source((smc_expr_id_t)id);
        size_t arity = smc_expr_arity((smc_expr_id_t)id);

        /* Allocate argument buffer if needed */
        double *args = NULL;
        if (arity > 0) {
            args = (double *)calloc(arity, sizeof(double));
            for (size_t i = 0; i < arity; i++) {
                args[i] = (double)(i + 1);
            }
        }

        volatile double accumulator = 0.0;
        clock_gettime(CLOCK_MONOTONIC, &start);
        for (int i = 0; i < iterations; i++) {
            double out = 0.0;
            (void)smc_call_double((smc_expr_id_t)id, args, arity, &out);
            accumulator += out;
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        double elapsed = (end.tv_sec - start.tv_sec) +
                         (end.tv_nsec - start.tv_nsec) / 1e9;

        printf("%-4d %-35s %-6zu %12.6f %12.0f\n",
               id, source ? source : "(null)", arity,
               elapsed, iterations / elapsed);

        free(args);
        (void)accumulator;
    }

    smc_shutdown();
    return 0;
}