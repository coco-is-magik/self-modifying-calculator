/* examples/c/hello_smc.c — minimal C example using the stub runtime */
#include "smc.h"
#include <stdio.h>

int main(void) {
    int rc = smc_init();
    if (rc != 0) {
        fprintf(stderr, "smc_init failed: %s\n", smc_error_string(rc));
        return 1;
    }

    double result;

    rc = smc_eval_double("2 + 3 * 4", &result);
    if (rc != 0) {
        fprintf(stderr, "eval failed: %s\n", smc_last_error()->message);
        smc_shutdown();
        return 1;
    }
    printf("2 + 3 * 4 = %f\n", result);

    rc = smc_eval_double("(1 + 2) ^ 3", &result);
    if (rc != 0) {
        fprintf(stderr, "eval failed: %s\n", smc_last_error()->message);
        smc_shutdown();
        return 1;
    }
    printf("(1 + 2) ^ 3 = %f\n", result);

    smc_context_t *ctx = smc_context_create(2);
    rc = smc_eval_double_with(ctx, "10 - 4 / 2", &result);
    if (rc != 0) {
        fprintf(stderr, "context eval failed: %s\n", smc_last_error()->message);
        smc_context_destroy(ctx);
        smc_shutdown();
        return 1;
    }
    printf("10 - 4 / 2 = %f\n", result);
    smc_context_destroy(ctx);

    smc_shutdown();
    return 0;
}
