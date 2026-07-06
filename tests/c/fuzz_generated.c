/* tests/c/fuzz_generated.c — Fuzz target for the generated dispatch table.
 *
 * Exercises smc_call_double with random expression IDs, argument counts,
 * and argument values to detect crashes, out-of-bounds access, or
 * undefined behavior.
 *
 * Build with clang and libFuzzer:
 *   clang -std=c99 -fsanitize=fuzzer,address -Iinclude \
 *       src/c/smc_runtime_stub.c src/c/smc_generated_runtime.c build/smc_generated.c \
 *       tests/c/fuzz_generated.c -o build/fuzz_generated -lm
 *
 * Or run with AFL:
 *   afl-clang-fast -std=c99 -Iinclude \
 *       src/c/smc_runtime_stub.c src/c/smc_generated_runtime.c build/smc_generated.c \
 *       tests/c/fuzz_generated.c -o build/fuzz_generated -lm
 */

#include "smc.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static int initialized = 0;
    if (!initialized) {
        smc_init();
        initialized = 1;
    }

    /* Use the first 4 bytes as expression ID, next 4 as argument count,
     * and the rest as argument data. */
    if (size < 8) {
        return 0;
    }

    uint32_t expr_id;
    uint32_t argc;
    memcpy(&expr_id, data, 4);
    memcpy(&argc, data + 4, 4);

    /* Clamp argc to a reasonable range to avoid excessive allocation */
    if (argc > 64) {
        argc = 64;
    }

    /* Use remaining bytes as double arguments, zero-padded */
    size_t args_bytes = (size - 8);
    size_t max_doubles = args_bytes / sizeof(double);
    size_t actual_args = (argc < max_doubles) ? argc : max_doubles;

    double *args = NULL;
    if (actual_args > 0) {
        args = (double *)malloc(actual_args * sizeof(double));
        if (args) {
            memcpy(args, data + 8, actual_args * sizeof(double));
        }
    }

    double out = 0.0;
    (void)smc_call_double((smc_expr_id_t)expr_id, args, actual_args, &out);

    /* Also exercise metadata functions with the same ID */
    (void)smc_expr_arity((smc_expr_id_t)expr_id);
    (void)smc_expr_source((smc_expr_id_t)expr_id);

    free(args);
    return 0;
}