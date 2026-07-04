/* tests/c/test_symbol_visibility.c — regression test for public ABI exports.
 *
 * The runtime must export the stable C ABI symbols so that dynamic-language
 * bindings (Python ctypes, etc.) can resolve them at load time. This test
 * links against libsmc and calls a representative subset of the public API.
 */

#include "smc.h"

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int rc;
    double value;

    /* Introspection functions are safe before smc_init(). */
    if (smc_abi_version() != SMC_ABI_VERSION) {
        fprintf(stderr, "FAIL: ABI version mismatch\n");
        return 1;
    }

    const char *kind = smc_runtime_kind();
    if (!kind) {
        fprintf(stderr, "FAIL: smc_runtime_kind returned NULL\n");
        return 1;
    }

    /* Lifecycle and Tier 1 evaluation. */
    rc = smc_init();
    if (rc != SMC_OK) {
        fprintf(stderr, "FAIL: smc_init returned %d\n", rc);
        return 1;
    }

    rc = smc_eval_double("2 + 3 * 4", &value);
    if (rc != SMC_OK) {
        fprintf(stderr, "FAIL: smc_eval_double returned %d\n", rc);
        return 1;
    }
    if (value != 14.0) {
        fprintf(stderr, "FAIL: expected 14.0, got %f\n", value);
        return 1;
    }

    /* Tier 2 metadata helpers must be reachable even if no table is linked. */
    (void)smc_expr_count();
    (void)smc_expr_arity(0);
    (void)smc_expr_source(0);

    /* Variables. */
    rc = smc_set_variable_double("x", 3.0);
    if (rc != SMC_OK) {
        fprintf(stderr, "FAIL: smc_set_variable_double returned %d\n", rc);
        return 1;
    }

    rc = smc_eval_double("x + 2", &value);
    if (rc != SMC_OK || value != 5.0) {
        fprintf(stderr, "FAIL: variable evaluation returned %d / %f\n", rc, value);
        return 1;
    }

    /* Observability. */
    smc_stats_t stats;
    rc = smc_get_stats(&stats);
    if (rc != SMC_OK) {
        fprintf(stderr, "FAIL: smc_get_stats returned %d\n", rc);
        return 1;
    }

    rc = smc_reset_stats();
    if (rc != SMC_OK) {
        fprintf(stderr, "FAIL: smc_reset_stats returned %d\n", rc);
        return 1;
    }

    rc = smc_shutdown();
    if (rc != SMC_OK) {
        fprintf(stderr, "FAIL: smc_shutdown returned %d\n", rc);
        return 1;
    }

    printf("PASS: symbol visibility\n");
    return 0;
}
