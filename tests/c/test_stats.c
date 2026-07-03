/* tests/c/test_stats.c — observability counters and statistics API tests */
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
    int rc = smc_init();
    CHECK(rc == SMC_OK);

    /* Reset and verify zero state */
    rc = smc_reset_stats();
    CHECK(rc == SMC_OK);

    smc_stats_t stats;
    memset(&stats, 0xff, sizeof(stats));
    rc = smc_get_stats(&stats);
    CHECK(rc == SMC_OK);
    CHECK(stats.total_calls == 0);
    CHECK(stats.generated_hits == 0);
    CHECK(stats.fallback_evals == 0);
    CHECK(stats.invalid_ids == 0);
    CHECK(stats.arity_errors == 0);
    CHECK(stats.invalid_calls == 0);
    CHECK(stats.parse_errors == 0);
    CHECK(stats.last_error_code == 0);

    /* Null output pointer is rejected */
    rc = smc_get_stats(NULL);
    CHECK(rc == SMC_ERR_INVALID);

    /* Tier 1 parse error should be recorded */
    double d;
    rc = smc_eval_double("1 + * 2", &d);
    CHECK(rc == SMC_ERR_PARSE);
    rc = smc_get_stats(&stats);
    CHECK(rc == SMC_OK);
    CHECK(stats.parse_errors >= 1);
    CHECK(stats.last_error_code == SMC_ERR_PARSE);

    /* Tier 2 fallback in stub runtime */
    rc = smc_reset_stats();
    CHECK(rc == SMC_OK);
    rc = smc_call_double(1, NULL, 0, &d);
    CHECK(rc == SMC_ERR_NOT_IMPL);
    rc = smc_get_stats(&stats);
    CHECK(rc == SMC_OK);
    CHECK(stats.total_calls == 1);
    CHECK(stats.fallback_evals == 1);

    /* Generated dispatch table path (if present) */
    int count = smc_expr_count();
    if (count > 0) {
        rc = smc_reset_stats();
        CHECK(rc == SMC_OK);

        rc = smc_call_double(1, NULL, 0, &d);
        CHECK(rc == SMC_OK);
        rc = smc_get_stats(&stats);
        CHECK(rc == SMC_OK);
        CHECK(stats.total_calls == 1);
        CHECK(stats.generated_hits == 1);
        CHECK(stats.fallback_evals == 0);

        /* Unknown ID */
        rc = smc_call_double((smc_expr_id_t)(count + 1), NULL, 0, &d);
        CHECK(rc == SMC_ERR_NOT_FOUND);
        rc = smc_get_stats(&stats);
        CHECK(rc == SMC_OK);
        CHECK(stats.total_calls == 2);
        CHECK(stats.invalid_ids == 1);

        /* Wrong arity */
        double args[1] = {1.0};
        rc = smc_call_double(1, args, 1, &d);
        CHECK(rc == SMC_ERR_ARITY);
        rc = smc_get_stats(&stats);
        CHECK(rc == SMC_OK);
        CHECK(stats.total_calls == 3);
        CHECK(stats.arity_errors == 1);

        /* Null output pointer */
        rc = smc_call_double(1, NULL, 0, NULL);
        CHECK(rc == SMC_ERR_INVALID);
        rc = smc_get_stats(&stats);
        CHECK(rc == SMC_OK);
        CHECK(stats.total_calls == 4);
        CHECK(stats.invalid_calls == 1);
    }

    rc = smc_shutdown();
    CHECK(rc == SMC_OK);

    printf("All stats tests passed.\n");
    return 0;
}
