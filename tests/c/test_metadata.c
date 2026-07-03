/* tests/c/test_metadata.c — expression metadata and error handling tests */
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

    /* ABI version */
    CHECK(smc_abi_version() == SMC_ABI_VERSION);
    CHECK(smc_abi_version() > 0);

    /* Runtime kind */
    const char *kind = smc_runtime_kind();
    CHECK(kind != NULL);
    CHECK(strlen(kind) > 0);

    /* Error strings */
    CHECK(strcmp(smc_error_string(SMC_OK), "success") == 0);
    CHECK(strstr(smc_error_string(SMC_ERR_PARSE), "parse") != NULL);
    CHECK(strstr(smc_error_string(SMC_ERR_EVAL), "evaluation") != NULL);
    CHECK(strstr(smc_error_string(SMC_ERR_NOT_IMPL), "not implemented") != NULL);
    CHECK(strstr(smc_error_string(SMC_ERR_INVALID), "argument") != NULL);
    CHECK(strstr(smc_error_string(SMC_ERR_ABI), "ABI") != NULL);
    CHECK(strstr(smc_error_string(SMC_ERR_ARITY), "arity") != NULL);
    CHECK(strstr(smc_error_string(SMC_ERR_NOT_FOUND), "not found") != NULL);
    CHECK(strstr(smc_error_string(SMC_ERR_THREAD), "thread") != NULL);
    CHECK(strstr(smc_error_string(SMC_ERR_SHUTDOWN), "shut down") != NULL);
    CHECK(strcmp(smc_error_string(-999), "unknown error") == 0);

    /* Last error */
    rc = smc_eval_double("1/0", NULL);
    CHECK(rc == SMC_ERR_INVALID);
    const smc_error_t *err = smc_last_error();
    CHECK(err != NULL);
    CHECK(err->code == SMC_ERR_INVALID);
    CHECK(strlen(err->message) > 0);

    /* Expression count */
    int count = smc_expr_count();
    CHECK(count >= 0);

    /* Metadata for invalid IDs */
    CHECK(smc_expr_source(0) == NULL);
    CHECK(smc_expr_arity(0) == 0);
    CHECK(smc_expr_source((smc_expr_id_t)(count + 999)) == NULL);
    CHECK(smc_expr_arity((smc_expr_id_t)(count + 999)) == 0);

    rc = smc_shutdown();
    CHECK(rc == SMC_OK);

    printf("All metadata tests passed.\n");
    return 0;
}
