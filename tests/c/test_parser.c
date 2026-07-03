/* tests/c/test_parser.c — parser correctness and failure-path tests */
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

    rc = smc_init();
    CHECK(rc == SMC_OK);

    /* Precedence */
    rc = smc_eval_double("2 + 3 * 4", &d);
    CHECK(rc == SMC_OK);
    CHECK(approx_eq(d, 14.0));

    /* Parentheses override precedence */
    rc = smc_eval_double("(2 + 3) * 4", &d);
    CHECK(rc == SMC_OK);
    CHECK(approx_eq(d, 20.0));

    /* Right-associative power */
    rc = smc_eval_double("2 ^ 3 ^ 2", &d);
    CHECK(rc == SMC_OK);
    CHECK(approx_eq(d, 512.0));

    /* Unary minus */
    rc = smc_eval_double("-5 + 3", &d);
    CHECK(rc == SMC_OK);
    CHECK(approx_eq(d, -2.0));

    /* Unary plus */
    rc = smc_eval_double("+7", &d);
    CHECK(rc == SMC_OK);
    CHECK(approx_eq(d, 7.0));

    /* Whitespace tolerance */
    rc = smc_eval_double("  2  +   3  ", &d);
    CHECK(rc == SMC_OK);
    CHECK(approx_eq(d, 5.0));

    /* Decimal numbers */
    rc = smc_eval_double("1.5 * 2.0", &d);
    CHECK(rc == SMC_OK);
    CHECK(approx_eq(d, 3.0));

    /* Malformed input */
    rc = smc_eval_double("2 + * 3", &d);
    CHECK(rc == SMC_ERR_PARSE);

    /* Empty expression */
    rc = smc_eval_double("", &d);
    CHECK(rc == SMC_ERR_PARSE);

    /* Trailing characters */
    rc = smc_eval_double("1+1 ", &d);
    CHECK(rc == SMC_OK);
    rc = smc_eval_double("1+1a", &d);
    CHECK(rc == SMC_ERR_PARSE);

    /* Division by zero */
    rc = smc_eval_double("1 / 0", &d);
    CHECK(rc == SMC_ERR_EVAL);

    /* Null expression pointer */
    rc = smc_eval_double(NULL, &d);
    CHECK(rc == SMC_ERR_INVALID);

    /* Null output pointer */
    rc = smc_eval_double("1+1", NULL);
    CHECK(rc == SMC_ERR_INVALID);

    /* Error string coverage */
    CHECK(strstr(smc_error_string(SMC_ERR_PARSE), "parse") != NULL);
    CHECK(strstr(smc_error_string(SMC_ERR_EVAL), "evaluation") != NULL);
    CHECK(strstr(smc_error_string(SMC_ERR_INVALID), "argument") != NULL);

    rc = smc_shutdown();
    CHECK(rc == SMC_OK);

    printf("All parser tests passed.\n");
    return 0;
}
