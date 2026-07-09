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
#include <time.h>

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

/* Verify we have a diverse set of expressions with different arities. */
static int test_expression_diversity(int count) {
    int ground_count = 0, unary_count = 0, binary_count = 0;
    
    for (int id = 1; id <= count; id++) {
        size_t arity = smc_expr_arity((smc_expr_id_t)id);
        if (arity == 0) ground_count++;
        else if (arity == 1) unary_count++;
        else if (arity == 2) binary_count++;
    }
    
    if (ground_count == 0) {
        fprintf(stderr, "FAIL: no ground expressions found\n");
        return 1;
    }
    if (unary_count == 0) {
        fprintf(stderr, "FAIL: no unary expressions found\n");
        return 1;
    }
    if (binary_count == 0) {
        fprintf(stderr, "FAIL: no binary expressions found\n");
        return 1;
    }
    
    printf("  Expression diversity: ground=%d unary=%d binary=%d\n", 
           ground_count, unary_count, binary_count);
    return 0;
}

/* Test edge cases: negative values, large values, and mixed signs. */
static int test_edge_cases(int count) {
    for (int id = 1; id <= count; id++) {
        size_t arity = smc_expr_arity((smc_expr_id_t)id);
        const char *source = smc_expr_source((smc_expr_id_t)id);
        
        if (arity == 0) continue; /* Only test argumentized expressions */
        
        /* Test cases: negative, large, and mixed sign values */
        double test_values[][2] = {
            {-3.0,  -2.0},   /* Both negative */
            {-3.0,   2.0},   /* One negative */
            { 3.0,  -2.0},   /* One negative */
            {1e6,    1e6},    /* Large values */
            {-1e6,  -1e6},    /* Large negative values */
            {0.0,    0.0},    /* Zero values */
        };
        
        for (int t = 0; t < 6; t++) {
            double actual = 0.0;
            double args[2] = {test_values[t][0], test_values[t][1]};
            
            int rc = smc_call_double((smc_expr_id_t)id,
                                     args, arity, &actual);
            if (rc != SMC_OK) {
                fprintf(stderr, "FAIL: edge case smc_call_double(%d) returned %d\n", id, rc);
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
                fprintf(stderr, "FAIL: edge case smc_eval_double(%s) returned %d\n", source, rc);
                return 1;
            }
            
            if (fabs(actual - expected) > 1e-6) {
                fprintf(stderr, "FAIL: id=%d edge case mismatch: actual=%g expected=%g\n",
                        id, actual, expected);
                return 1;
            }
        }
    }
    
    printf("  Edge case tests passed\n");
    return 0;
}

/* Test random argument values for stress testing. */
static int test_random_arguments(int count) {
    const int NUM_RANDOM = 50;
    srand((unsigned)time(NULL));
    
    for (int id = 1; id <= count; id++) {
        size_t arity = smc_expr_arity((smc_expr_id_t)id);
        const char *source = smc_expr_source((smc_expr_id_t)id);
        
        if (arity == 0) continue; /* Only stress-test argumentized expressions */
        
        for (int t = 0; t < NUM_RANDOM; t++) {
            /* Generate random values in range [-1000, 1000] */
            double x = ((double)rand() / RAND_MAX) * 2000.0 - 1000.0;
            double y = ((double)rand() / RAND_MAX) * 2000.0 - 1000.0;
            double args[2] = {x, y};
            double actual = 0.0;
            
            int rc = smc_call_double((smc_expr_id_t)id, args, arity, &actual);
            if (rc != SMC_OK) {
                fprintf(stderr, "FAIL: random smc_call_double(%d) returned %d\n", id, rc);
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
                fprintf(stderr, "FAIL: random smc_eval_double(%s) returned %d\n", source, rc);
                return 1;
            }
            
            if (fabs(actual - expected) > 1e-6) {
                fprintf(stderr, "FAIL: id=%d random mismatch: actual=%g expected=%g (x=%g y=%g)\n",
                        id, actual, expected, args[0], args[1]);
                return 1;
            }
        }
    }
    
    printf("  Random stress test passed: %d iterations per argumentized expr\n", NUM_RANDOM);
    return 0;
}

/* Cross-check a generated expression against Tier 1. */
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

    /* Verify expression diversity */
    if (test_expression_diversity(count) != 0) {
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

    /* Test edge cases and random values for stress validation */
    if (test_edge_cases(count) != 0) {
        return 1;
    }
    
    if (test_random_arguments(count) != 0) {
        return 1;
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