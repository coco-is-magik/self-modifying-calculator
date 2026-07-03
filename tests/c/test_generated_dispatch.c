/* tests/c/test_generated_dispatch.c — generated dispatch failure-path tests */
#include "smc.h"
#include <math.h>
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

    int count = smc_expr_count();
    if (count == 0) {
        printf("SKIP: test_generated_dispatch (no generated dispatch table)\n");
        return 0;
    }

    /* Valid ID, ground expression */
    rc = smc_call_double(1, NULL, 0, &d);
    CHECK(rc == SMC_OK);

    /* Invalid IDs */
    rc = smc_call_double(0, NULL, 0, &d);
    CHECK(rc == SMC_ERR_NOT_FOUND);
    rc = smc_call_double((smc_expr_id_t)(count + 1), NULL, 0, &d);
    CHECK(rc == SMC_ERR_NOT_FOUND);

    /* Null output pointer */
    rc = smc_call_double(1, NULL, 0, NULL);
    CHECK(rc == SMC_ERR_INVALID);

    /* Wrong arity for a ground expression */
    double args[1] = {1.0};
    rc = smc_call_double(1, args, 1, &d);
    CHECK(rc == SMC_ERR_ARITY);

    /* Metadata */
    const char *src = smc_expr_source(1);
    CHECK(src != NULL);
    CHECK(strlen(src) > 0);
    CHECK(smc_expr_arity(1) == 0 || smc_expr_arity(1) > 0); /* arity is 0 or more */
    CHECK(smc_expr_arity(0) == 0);
    CHECK(smc_expr_source(0) == NULL);

    /* Cross-check generated result against Tier 1 */
    for (int id = 1; id <= count; id++) {
        const char *expr = smc_expr_source(id);
        CHECK(expr != NULL);
        double expected;
        rc = smc_eval_double(expr, &expected);
        CHECK(rc == SMC_OK);
        double actual;
        rc = smc_call_double((smc_expr_id_t)id, NULL, 0, &actual);
        CHECK(rc == SMC_OK);
        CHECK(fabs(actual - expected) < 1e-9);
    }

    rc = smc_shutdown();
    CHECK(rc == SMC_OK);

    printf("All generated dispatch tests passed.\n");
    return 0;
}
