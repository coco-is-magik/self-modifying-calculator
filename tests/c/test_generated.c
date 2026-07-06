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

/* Cross-check a generated expression against Tier 1.
 * For ground expressions, pass no args. For argumentized expressions,
 * bind variables in the global context and pass matching args.
 *
 * The generator orders variables by left-to-right occurrence, deduplicated.
 * For the default generated set that means:
 *   arity 1: x is the only variable -> args[0] is x
 *   arity 2: x occurs before y      -> args[0] is x, args[1] is y
 */
static int cross_check_expression(int id, const char *source, size_t arity) {
    double actual = 0.0;
    double args[2] = {3.2, 2.1};
    int rc = smc_call_double((smc_expr_id_t)id,
                             (arity == 0) ? NULL : args,
                             arity,
                             &actual);
    if (rc != SMC_OK) {
        fprintf(stderr, "FAIL: smc_call_double(%d) returned %d\n", id, rc);
        return 1;
    }

    smc_clear_variables();
    if (arity >= 1) {
        rc = smc_set_variable_double("x", args[0]);
        if (rc != SMC_OK) {
            fprintf(stderr, "FAIL: smc_set_variable_double(x) returned %d\n", rc);
            return 1;
        }
    }
    if (arity >= 2) {
        rc = smc_set_variable_double("y", args[1]);
        if (rc != SMC_OK) {
            fprintf(stderr, "FAIL: smc_set_variable_double(y) returned %d\n", rc);
            return 1;
        }
    }

    double expected = 0.0;
    rc = smc_eval_double(source, &expected);
    if (rc != SMC_OK) {
        fprintf(stderr, "FAIL: smc_eval_double(%s) returned %d\n", source, rc);
        return 1;
    }

    if (fabs(actual - expected) > 1e-9) {
        fprintf(stderr, "FAIL: id=%d source=%s actual=%g expected=%g\n",
                id, source, actual, expected);
        return 1;
    }
    return 0;
}

int main(void) {
    int rc = smc_init();
    ASSERT_OK(rc, "smc_init");

    int count = smc_expr_count();
    printf("Generated expressions: %d\n", count);
    if (count <= 0) {
        fprintf(stderr, "FAIL: no generated expressions found\n");
        return 1;
    }

    /* Evaluate each generated expression by ID and cross-check against Tier 1. */
    for (int id = 1; id <= count; id++) {
        const char *source = smc_expr_source((smc_expr_id_t)id);
        if (!source) {
            fprintf(stderr, "FAIL: smc_expr_source(%d) returned NULL\n", id);
            return 1;
        }
        size_t arity = smc_expr_arity((smc_expr_id_t)id);
        printf("  id=%d arity=%zu source=%s\n", id, arity, source);

        if (cross_check_expression(id, source, arity) != 0) {
            return 1;
        }

        /* For argumentized expressions, also test a second binding set to
         * ensure the generated code is not hard-coded to the warm-cache values. */
        if (arity > 0) {
            double args2[2] = {1.5, 2.5};
            double actual2 = 0.0;
            rc = smc_call_double((smc_expr_id_t)id,
                                 args2, arity, &actual2);
            if (rc != SMC_OK) {
                fprintf(stderr, "FAIL: second smc_call_double(%d) returned %d\n", id, rc);
                return 1;
            }
            smc_clear_variables();
            if (arity >= 1) {
                rc = smc_set_variable_double("x", args2[0]);
                if (rc != SMC_OK) {
                    fprintf(stderr, "FAIL: smc_set_variable_double(x) returned %d\n", rc);
                    return 1;
                }
            }
            if (arity >= 2) {
                rc = smc_set_variable_double("y", args2[1]);
                if (rc != SMC_OK) {
                    fprintf(stderr, "FAIL: smc_set_variable_double(y) returned %d\n", rc);
                    return 1;
                }
            }
            double expected2 = 0.0;
            rc = smc_eval_double(source, &expected2);
            if (rc != SMC_OK) {
                fprintf(stderr, "FAIL: second smc_eval_double(%s) returned %d\n", source, rc);
                return 1;
            }
            if (fabs(actual2 - expected2) > 1e-9) {
                fprintf(stderr, "FAIL: id=%d source=%s actual2=%g expected2=%g\n",
                        id, source, actual2, expected2);
                return 1;
            }
        }
    }

    /* Verify invalid ID returns SMC_ERR_NOT_FOUND. */
    double dummy = 0.0;
    rc = smc_call_double((smc_expr_id_t)0, NULL, 0, &dummy);
    ASSERT_EQ(rc, SMC_ERR_NOT_FOUND, "invalid expr_id 0");

    rc = smc_call_double((smc_expr_id_t)(count + 1), NULL, 0, &dummy);
    ASSERT_EQ(rc, SMC_ERR_NOT_FOUND, "invalid expr_id count+1");

    /* Verify wrong arity returns SMC_ERR_ARITY. Pick a ground expression if
     * one exists; otherwise use the first expression and pass one extra arg. */
    int ground_id = 0;
    for (int id = 1; id <= count; id++) {
        if (smc_expr_arity((smc_expr_id_t)id) == 0) {
            ground_id = id;
            break;
        }
    }
    double args[1] = {1.0};
    if (ground_id != 0) {
        rc = smc_call_double((smc_expr_id_t)ground_id, args, 1, &dummy);
        ASSERT_EQ(rc, SMC_ERR_ARITY, "wrong arity for ground expr");
    } else {
        rc = smc_call_double((smc_expr_id_t)1, args, smc_expr_arity(1) + 1, &dummy);
        ASSERT_EQ(rc, SMC_ERR_ARITY, "wrong arity for expr 1");
    }

    rc = smc_shutdown();
    ASSERT_OK(rc, "smc_shutdown");

    printf("All generated-code tests passed.\n");
    return 0;
}
