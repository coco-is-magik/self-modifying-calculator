/* tests/benchmarks/benchmark_cold_startup.c
 * Measure smc_init() to first smc_call_double() latency.
 *
 * Build with CMake (SMC_BUILD_BENCHMARKS) or manually:
 *   gcc -std=c99 -O2 -Iinclude \
 *       src/c/smc_runtime_stub.c src/c/smc_generated_runtime.c build/smc_generated.c \
 *       tests/benchmarks/benchmark_cold_startup.c -o build/benchmark_cold_startup -lm
 */

#define _POSIX_C_SOURCE 200809L

#include "smc.h"

#include <stdio.h>
#include <time.h>

int main(void) {
    const int trials = 100;
    double min_time = 1e9, max_time = 0.0, total_time = 0.0;

    printf("=== Cold Startup Benchmark ===\n");
    printf("Trials: %d\n\n", trials);

    for (int t = 0; t < trials; t++) {
        struct timespec start, end;

        clock_gettime(CLOCK_MONOTONIC, &start);

        int rc = smc_init();
        if (rc != SMC_OK) {
            fprintf(stderr, "smc_init failed on trial %d\n", t);
            return 1;
        }

        /* First smc_call_double (may be fallback if no generated table) */
        double out = 0.0;
        (void)smc_call_double(1, NULL, 0, &out);

        smc_shutdown();

        clock_gettime(CLOCK_MONOTONIC, &end);

        double elapsed = (end.tv_sec - start.tv_sec) * 1e9 +
                         (end.tv_nsec - start.tv_nsec);

        if (elapsed < min_time) min_time = elapsed;
        if (elapsed > max_time) max_time = elapsed;
        total_time += elapsed;
    }

    double avg_time = total_time / trials;

    printf("Init + first call latency:\n");
    printf("  Average: %.0f ns (%.6f ms)\n", avg_time, avg_time / 1e6);
    printf("  Minimum: %.0f ns (%.6f ms)\n", min_time, min_time / 1e6);
    printf("  Maximum: %.0f ns (%.6f ms)\n", max_time, max_time / 1e6);

    /* Report whether a generated dispatch table was available */
    smc_init();
    int count = smc_expr_count();
    printf("\nGenerated table: %s (%d expressions)\n",
           count > 0 ? "present" : "absent", count);
    smc_shutdown();

    return 0;
}