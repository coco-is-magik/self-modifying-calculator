/* tests/c/test_generated.c — acceptance test for generated-code Tier 2 API */
/*
 * This test links against the stub runtime plus a generated dispatch table.
 * It verifies that:
 *   - smc_init / smc_shutdown work
 *   - smc_expr_count returns the number of generated expressions
 *   - smc_call_double evaluates cached expressions by ID
 *   - smc_expr_source returns the original expression string
 *   - smc_eval_double still works for Tier 1 fallback
 *
 * Build with:
 *   gcc -Iinclude -o tests/c/test_generated \
 *       tests/c/test_generated.c \
 *       src/c/smc_runtime_stub.c src/c/smc_generated_runtime.c \
 *       build/smc_generated.c -lm
 */

#include "smc.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT_EQ(actual, expected, msg) \
    do { \
        if ((actual) != (expected)) { \
            fprintf(stderr, "FAIL: %s: expected %lld, got %lld\n", msg, \
                    (long long)(expected), (long long)(actual)); \
            return 1; \
        } \
    } while (0)

#define ASSERT_NEAR(actual, expected, tol, msg) \
    do { \
        if (fabs((actual) - (expected)) > (tol)) { \
            fprintf(stderr, "FAIL: %s: expected %f, got %f\n", msg, \
                    (double)(expected), (double)(actual)); \
            return 1; \
        } \
    } while (0)

#define ASSERT_OK(rc, msg) \
    do { \
        if ((rc) != SMC_OK) { \
            const smc_error_t *err = smc_last_error(); \
            fprintf(stderr, "FAIL: %s: rc=%d (%s)\n", msg, rc, \
                    err ? err->message : "unknown"); \
            return 1; \
        } \
    } while (0)

int main(void) {
    int rc = smc_init();
    ASSERT_OK(rc, "smc_init");

    int count = smc_expr_count();
    printf("Generated expressions: %d\n", count);
    ASSERT_EQ(count, 10, "smc_expr_count");

    /* Evaluate each generated expression by ID. */
    for (int id = 1; id <= count; id++) {
        double value = 0.0;
        rc = smc_call_double((smc_expr_id_t)id, NULL, 0, &value);
        ASSERT_OK(rc, "smc_call_double");

        const char *source = smc_expr_source((smc_expr_id_t)id);
        if (!source) {
            fprintf(stderr, "FAIL: smc_expr_source(%d) returned NULL\n", id);
            return 1;
        }
        printf("  id=%d source=%s value=%g\n", id, source, value);

        /* Cross-check against Tier 1 parser for the same expression. */
        double expected = 0.0;
        rc = smc_eval_double(source, &expected);
        ASSERT_OK(rc, "smc_eval_double cross-check");
        ASSERT_NEAR(value, expected, 1e-9, source);
    }

    /* Verify invalid ID returns SMC_ERR_INVALID. */
    double dummy = 0.0;
    rc = smc_call_double((smc_expr_id_t)0, NULL, 0, &dummy);
    ASSERT_EQ(rc, SMC_ERR_INVALID, "invalid expr_id 0");

    rc = smc_call_double((smc_expr_id_t)(count + 1), NULL, 0, &dummy);
    ASSERT_EQ(rc, SMC_ERR_INVALID, "invalid expr_id count+1");

    rc = smc_shutdown();
    ASSERT_OK(rc, "smc_shutdown");

    printf("All generated-code tests passed.\n");
    return 0;
}
