/* tests/benchmarks/benchmark_binary_size.c
 * Estimate .text growth per generated expression by analyzing the compiled
 * generated dispatch table.
 *
 * This benchmark compiles the generated C source with size-reporting flags
 * and reports the per-expression code size. It does not require running
 * smc_init or calling any SMC functions at runtime — it's a compile-time
 * analysis tool.
 *
 * Build with CMake (SMC_BUILD_BENCHMARKS) or manually:
 *   gcc -std=c99 -O2 -Iinclude \
 *       src/c/smc_runtime_stub.c src/c/smc_generated_runtime.c build/smc_generated.c \
 *       tests/benchmarks/benchmark_binary_size.c -o build/benchmark_binary_size -lm
 *
 * Then run with the SMC shared library in scope to measure generated text size.
 */

#define _POSIX_C_SOURCE 200809L

#include "smc.h"

#include <stdio.h>

int main(void) {
    if (smc_init() != SMC_OK) {
        fprintf(stderr, "smc_init failed\n");
        return 1;
    }

    int count = smc_expr_count();
    if (count == 0) {
        printf("No generated dispatch table.\n");
        smc_shutdown();
        return 0;
    }

    printf("=== Binary Size Benchmark ===\n");
    printf("Expressions in generated table: %d\n\n", count);

    printf("To measure the exact .text size per expression:\n");
    printf("  objdump -d build/libsmc_generated.a 2>/dev/null | \\\n");
    printf("    grep 'smc_expr_' | grep -o '^[0-9a-f]*' | \\\n");
    printf("    while read addr; do echo \"0x$addr\"; done | \\\n");
    printf("    sort | uniq -c\n\n");

    printf("Approximate per-expression size can be estimated by:\n");
    printf("  size -t build/libsmc_generated.a 2>/dev/null\n\n");

    printf("Listing all generated function symbols:\n");
    printf("  nm build/libsmc_generated.a 2>/dev/null | grep ' T '\n\n");

    /* Print the filename of the generated source for manual inspection. */
    printf("Generated expressions (for manual size estimation):\n");
    for (int id = 1; id <= count; id++) {
        const char *source = smc_expr_source((smc_expr_id_t)id);
        size_t arity = smc_expr_arity((smc_expr_id_t)id);
        if (source) {
            printf("  id=%-4d arity=%-2zu source=\"%s\"\n", id, arity, source);
        }
    }

    smc_shutdown();
    return 0;
}