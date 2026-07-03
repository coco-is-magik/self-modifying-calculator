/* tests/c/test_context_isolation.c — context isolation tests */
#include "smc.h"
#include <stdio.h>
#include <string.h>

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s at line %d\n", #cond, __LINE__); \
        return 1; \
    } \
} while (0)

int main(void) {
    int rc;
    double d;

    rc = smc_init();
    CHECK(rc == SMC_OK);

    /* Global variable */
    rc = smc_set_variable_double("x", 10.0);
    CHECK(rc == SMC_OK);

    /* Context variable shadows global */
    smc_context_t *ctx = smc_context_create(1);
    CHECK(ctx != NULL);
    rc = smc_set_variable_double_with(ctx, "x", 20.0);
    CHECK(rc == SMC_OK);

    rc = smc_eval_double_with(ctx, "x", &d);
    CHECK(rc == SMC_OK);
    CHECK(d == 20.0);

    rc = smc_eval_double("x", &d);
    CHECK(rc == SMC_OK);
    CHECK(d == 10.0);

    /* Clear context variables only */
    rc = smc_clear_variables_with(ctx);
    CHECK(rc == SMC_OK);
    rc = smc_eval_double_with(ctx, "x", &d);
    CHECK(rc == SMC_ERR_EVAL);

    /* Global still intact */
    rc = smc_eval_double("x", &d);
    CHECK(rc == SMC_OK);
    CHECK(d == 10.0);

    /* Context optimization level isolation */
    smc_context_t *ctx2 = smc_context_create(3);
    CHECK(smc_get_optimization_level() == 1); /* global unchanged */
    (void)ctx2;
    smc_context_destroy(ctx2);

    smc_context_destroy(ctx);

    /* Clear global */
    rc = smc_clear_variables();
    CHECK(rc == SMC_OK);
    rc = smc_eval_double("x", &d);
    CHECK(rc == SMC_ERR_EVAL);

    rc = smc_shutdown();
    CHECK(rc == SMC_OK);

    printf("All context isolation tests passed.\n");
    return 0;
}
