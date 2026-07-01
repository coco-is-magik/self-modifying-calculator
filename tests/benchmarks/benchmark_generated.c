/* tests/benchmarks/benchmark_generated.c — compare Tier 1 vs Tier 2 in C */
/*
 * Build with CMake (SMC_BUILD_EXAMPLES/BENCHMARKS) or manually:
 *
 *   gcc -std=c99 -O2 -Iinclude \
 *       src/c/smc_runtime_stub.c src/c/smc_generated_runtime.c build/smc_generated.c \
 *       tests/benchmarks/benchmark_generated.c -o build/benchmark_generated -lm
 */

#define _POSIX_C_SOURCE 200809L

#include "smc.h"

#include <stdio.h>
#include <time.h>

static double bench_tier1(const char *expr, int iterations) {
    volatile double accumulator = 0.0;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < iterations; i++) {
        double out = 0.0;
        (void)smc_eval_double(expr, &out);
        accumulator += out;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    (void)accumulator;
    return elapsed;
}

static double bench_tier2(smc_expr_id_t id, int iterations) {
    volatile double accumulator = 0.0;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < iterations; i++) {
        double out = 0.0;
        (void)smc_call_double(id, NULL, 0, &out);
        accumulator += out;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    (void)accumulator;
    return elapsed;
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

    const int iterations = 1000000;

    for (int id = 1; id <= count; id++) {
        const char *source = smc_expr_source((smc_expr_id_t)id);
        if (!source) continue;

        double t1 = bench_tier1(source, iterations);
        double t2 = bench_tier2((smc_expr_id_t)id, iterations);

        printf("expr=%-30s tier1=%8.3f s  tier2=%8.3f s  speedup=%6.1fx\n",
               source, t1, t2, t1 / t2);
    }

    smc_shutdown();
    return 0;
}
