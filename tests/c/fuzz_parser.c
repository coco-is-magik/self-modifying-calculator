/* tests/c/fuzz_parser.c — Fuzz target for the stub runtime parser.
 *
 * Build with clang and libFuzzer:
 *   clang -std=c99 -fsanitize=fuzzer,address -Iinclude \
 *       src/c/smc_runtime_stub.c src/c/smc_generated_runtime.c \
 *       tests/c/fuzz_parser.c -o build/fuzz_parser -lm
 *
 * Or run with AFL:
 *   afl-clang-fast -std=c99 -Iinclude \
 *       src/c/smc_runtime_stub.c src/c/smc_generated_runtime.c \
 *       tests/c/fuzz_parser.c -o build/fuzz_parser -lm
 */

#include "smc.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* smc_init is not idempotent across fuzz iterations with libFuzzer.
     * We initialize once and test only the eval path. */
    static int initialized = 0;
    if (!initialized) {
        smc_init();
        initialized = 1;
    }

    /* Copy input and NUL-terminate */
    char *expr = (char *)malloc(size + 1);
    if (!expr) {
        return 0;
    }
    memcpy(expr, data, size);
    expr[size] = '\0';

    /* Exercise Tier 1: parse + evaluate */
    double out = 0.0;
    (void)smc_eval_double(expr, &out);

    /* Exercise Tier 2: fallback */
    (void)smc_call_double(1, NULL, 0, &out);

    free(expr);
    return 0; /* Non-zero return values are reserved for libFuzzer bugs */
}