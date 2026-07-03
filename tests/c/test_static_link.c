/* tests/c/test_static_link.c — verify static library symbol resolution */
#include "smc.h"
#include <stdio.h>

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s at line %d\n", #cond, __LINE__); \
        return 1; \
    } \
} while (0)

int main(void) {
    int rc = smc_init();
    CHECK(rc == SMC_OK);

    double d;
    rc = smc_eval_double("3 + 4", &d);
    CHECK(rc == SMC_OK);
    CHECK(d == 7.0);

    rc = smc_shutdown();
    CHECK(rc == SMC_OK);

    printf("Static link test passed.\n");
    return 0;
}
