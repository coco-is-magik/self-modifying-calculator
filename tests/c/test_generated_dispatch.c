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

    /* Find a ground expression for the simple failure-path checks. */
    int ground_id = 0;
    for (int id = 1; id <= count; id++) {
        if (smc_expr_arity((smc_expr_id_t)id) == 0) {
            ground_id = id;
            break;
        }
    }

    /* Valid ID, ground expression */
    if (ground_id != 0) {
        rc = smc_call_double((smc_expr_id_t)ground_id, NULL, 0, &d);
        CHECK(rc == SMC_OK);
    }

    /* Invalid IDs */
    rc = smc_call_double(0, NULL, 0, &d);
    CHECK(rc == SMC_ERR_NOT_FOUND);
    rc = smc_call_double((smc_expr_id_t)(count + 1), NULL, 0, &d);
    CHECK(rc == SMC_ERR_NOT_FOUND);

    /* Null output pointer */
    rc = smc_call_double(1, NULL, 0, NULL);
    CHECK(rc == SMC_ERR_INVALID);

    /* Wrong arity */
    double args[1] = {1.0};
    if (ground_id != 0) {
        rc = smc_call_double((smc_expr_id_t)ground_id, args, 1, &d);
        CHECK(rc == SMC_ERR_ARITY);
    } else {
        rc = smc_call_double(1, args, smc_expr_arity(1) + 1, &d);
        CHECK(rc == SMC_ERR_ARITY);
    }

    /* Metadata */
    const char *src = smc_expr_source(1);
    CHECK(src != NULL);
    CHECK(strlen(src) > 0);
    CHECK(smc_expr_arity(1) == 0 || smc_expr_arity(1) > 0); /* arity is 0 or more */
    CHECK(smc_expr_arity(0) == 0);
    CHECK(smc_expr_source(0) == NULL);

    /* Cross-check generated result against Tier 1.
     * For argumentized expressions, bind variables in the global context. */
    for (int id = 1; id <= count; id++) {
        const char *expr = smc_expr_source(id);
        CHECK(expr != NULL);
        size_t arity = smc_expr_arity((smc_expr_id_t)id);
        double args2[2] = {3.2, 2.1};
        double actual;
        rc = smc_call_double((smc_expr_id_t)id,
                             (arity == 0) ? NULL : args2,
                             arity,
                             &actual);
        CHECK(rc == SMC_OK);

        smc_clear_variables();
        if (arity >= 1) {
            rc = smc_set_variable_double("x", args2[0]);
            CHECK(rc == SMC_OK);
        }
        if (arity >= 2) {
            rc = smc_set_variable_double("y", args2[1]);
            CHECK(rc == SMC_OK);
        }
        double expected;
        rc = smc_eval_double(expr, &expected);
        CHECK(rc == SMC_OK);
        CHECK(fabs(actual - expected) < 1e-9);
    }

    rc = smc_shutdown();
    CHECK(rc == SMC_OK);

    printf("All generated dispatch tests passed.\n");
    return 0;
}
