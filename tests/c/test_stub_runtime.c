/* tests/c/test_stub_runtime.c — acceptance tests for the stub C runtime */
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

static int approx_eq(double a, double b) {
    return fabs(a - b) < 1e-9;
}

int main(void) {
    int rc;
    double d;
    float f;
    int64_t i;

    /* Lifecycle */
    rc = smc_init();
    CHECK(rc == 0);

    /* Basic evaluation */
    rc = smc_eval_double("2 + 3 * 4", &d);
    CHECK(rc == 0);
    CHECK(approx_eq(d, 14.0));

    /* Parentheses and precedence */
    rc = smc_eval_double("(2 + 3) * 4", &d);
    CHECK(rc == 0);
    CHECK(approx_eq(d, 20.0));

    /* Power (right-associative: 2^(3^2) = 512) */
    rc = smc_eval_double("2 ^ 3 ^ 2", &d);
    CHECK(rc == 0);
    CHECK(approx_eq(d, 512.0));

    /* Unary minus */
    rc = smc_eval_double("-5 + 3", &d);
    CHECK(rc == 0);
    CHECK(approx_eq(d, -2.0));

    /* Float variant */
    rc = smc_eval_float("1.5 * 2", &f);
    CHECK(rc == 0);
    CHECK(approx_eq(f, 3.0f));

    /* Int variant */
    rc = smc_eval_int("7 / 2", &i);
    CHECK(rc == 0);
    CHECK(i == 3);

    /* Context API */
    smc_context_t *ctx = smc_context_create(2);
    CHECK(ctx != NULL);
    rc = smc_eval_double_with(ctx, "10 - 4 / 2", &d);
    CHECK(rc == 0);
    CHECK(approx_eq(d, 8.0));
    smc_context_destroy(ctx);

    /* Optimization level */
    rc = smc_set_optimization_level(3);
    CHECK(rc == 0);
    CHECK(smc_get_optimization_level() == 3);

    /* Error handling: parse error */
    rc = smc_eval_double("2 + * 3", &d);
    CHECK(rc != 0);
    const smc_error_t *err = smc_last_error();
    CHECK(err != NULL);
    CHECK(err->code != 0);

    /* Error string */
    const char *msg = smc_error_string(SMC_ERR_PARSE);
    CHECK(msg != NULL);
    CHECK(strstr(msg, "parse") != NULL);

    /* Division by zero */
    rc = smc_eval_double("1 / 0", &d);
    CHECK(rc != 0);

    /* Tier 2 not implemented in stub */
    rc = smc_call_double(0, NULL, 0, &d);
    CHECK(rc != 0);

    /* Variables not implemented in stub */
    rc = smc_set_variable_double("x", 3.0);
    CHECK(rc != 0);

    /* Cache clear is a no-op in stub */
    rc = smc_cache_clear();
    CHECK(rc == 0);

    /* Source generation not implemented in stub */
    rc = smc_generate_c_source("/tmp/smc_generated.c");
    CHECK(rc != 0);

    rc = smc_shutdown();
    CHECK(rc == 0);

    printf("All C stub runtime tests passed.\n");
    return 0;
}
