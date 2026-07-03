/* tests/c/test_lifecycle.c — lifecycle and context tests for the SMC C API */
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

    /* Pre-init: only error_string and abi_version are safe. */
    CHECK(smc_abi_version() == SMC_ABI_VERSION);
    CHECK(strcmp(smc_runtime_kind(), "stub") == 0);
    CHECK(smc_error_string(SMC_OK) != NULL);

    /* Pre-init eval must fail. */
    rc = smc_eval_double("1+1", &d);
    CHECK(rc == SMC_ERR_INIT);
    const smc_error_t *err = smc_last_error();
    CHECK(err != NULL);
    CHECK(err->code == SMC_ERR_INIT);

    /* Init */
    rc = smc_init();
    CHECK(rc == SMC_OK);

    /* Double init is idempotent. */
    rc = smc_init();
    CHECK(rc == SMC_OK);

    /* Eval works after init. */
    rc = smc_eval_double("1+1", &d);
    CHECK(rc == SMC_OK);
    CHECK(d == 2.0);

    /* Shutdown */
    rc = smc_shutdown();
    CHECK(rc == SMC_OK);

    /* Post-shutdown eval fails. */
    rc = smc_eval_double("1+1", &d);
    CHECK(rc == SMC_ERR_INIT);

    /* Re-init works. */
    rc = smc_init();
    CHECK(rc == SMC_OK);

    /* Context create/destroy. */
    smc_context_t *ctx = smc_context_create(2);
    CHECK(ctx != NULL);
    rc = smc_eval_double_with(ctx, "3*3", &d);
    CHECK(rc == SMC_OK);
    CHECK(d == 9.0);
    smc_context_destroy(ctx);

    /* Null context destroy is safe. */
    smc_context_destroy(NULL);

    /* Null context eval fails. */
    rc = smc_eval_double_with(NULL, "1+1", &d);
    CHECK(rc == SMC_ERR_INIT);

    /* Optimization level */
    rc = smc_set_optimization_level(5);
    CHECK(rc == SMC_OK);
    CHECK(smc_get_optimization_level() == 3);
    rc = smc_set_optimization_level(0);
    CHECK(rc == SMC_OK);
    CHECK(smc_get_optimization_level() == 1);

    rc = smc_shutdown();
    CHECK(rc == SMC_OK);

    printf("All lifecycle tests passed.\n");
    return 0;
}
