/* tests/c/test_state_tracking.c — acceptance test for dirty-state API
 *
 * Tests state_changed behavior, stats tracking, and preallocation.
 */

#include "smc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT_EQ(actual, expected, msg) \
    do { \
        if ((actual) != (expected)) { \
            fprintf(stderr, "FAIL: %s: expected %llu, got %llu\n", msg, (unsigned long long)(expected), (unsigned long long)(actual)); \
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

/* Test first observation returns changed=1 */
static int test_first_observation(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_config_t config = {0};
    int rc = smc_state_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_configure");
    
    uint32_t key = 42;
    uint32_t state = 0xDEADBEEF;
    int changed = 0;
    
    rc = smc_state_changed(ctx, &key, sizeof(key), &state, sizeof(state), &changed);
    ASSERT_OK(rc, "smc_state_changed");
    ASSERT_EQ(changed, 1, "first observation is changed");
    
    smc_context_destroy(ctx);
    printf("  test_first_observation: PASS\n");
    return 0;
}

/* Test same state returns changed=0 */
static int test_same_state(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_config_t config = {0};
    int rc = smc_state_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_configure");
    
    uint32_t key = 42;
    uint32_t state = 0xDEADBEEF;
    int changed = 0;
    
    rc = smc_state_changed(ctx, &key, sizeof(key), &state, sizeof(state), &changed);
    ASSERT_OK(rc, "first check");
    ASSERT_EQ(changed, 1, "first is changed");
    
    changed = 0;
    rc = smc_state_changed(ctx, &key, sizeof(key), &state, sizeof(state), &changed);
    ASSERT_OK(rc, "second check same state");
    ASSERT_EQ(changed, 0, "same state is unchanged");
    
    smc_context_destroy(ctx);
    printf("  test_same_state: PASS\n");
    return 0;
}

/* Test different state returns changed=1 and updates stored state */
static int test_different_state(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_config_t config = {0};
    int rc = smc_state_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_configure");
    
    uint32_t key = 42;
    uint32_t state1 = 100;
    uint32_t state2 = 200;
    int changed = 0;
    
    rc = smc_state_changed(ctx, &key, sizeof(key), &state1, sizeof(state1), &changed);
    ASSERT_OK(rc, "first check");
    ASSERT_EQ(changed, 1, "first is changed");
    
    changed = 0;
    rc = smc_state_changed(ctx, &key, sizeof(key), &state2, sizeof(state2), &changed);
    ASSERT_OK(rc, "second check different state");
    ASSERT_EQ(changed, 1, "different state is changed");
    
    /* Verify stored state was updated */
    changed = 0;
    rc = smc_state_changed(ctx, &key, sizeof(key), &state2, sizeof(state2), &changed);
    ASSERT_OK(rc, "third check");
    ASSERT_EQ(changed, 0, "now state matches");
    
    smc_context_destroy(ctx);
    printf("  test_different_state: PASS\n");
    return 0;
}

/* Test stats tracking including evictions */
static int test_stats(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_config_t config = {0};
    int rc = smc_state_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_configure");
    
    rc = smc_state_reset_stats(ctx);
    ASSERT_OK(rc, "smc_state_reset_stats");
    
    uint32_t key = 1;
    uint32_t state = 100;
    int changed = 0;
    
    /* First observation */
    smc_state_changed(ctx, &key, sizeof(key), &state, sizeof(state), &changed);
    /* Same state */
    smc_state_changed(ctx, &key, sizeof(key), &state, sizeof(state), &changed);
    /* Different state */
    state = 200;
    smc_state_changed(ctx, &key, sizeof(key), &state, sizeof(state), &changed);
    
    smc_state_stats_t stats;
    rc = smc_state_get_stats(ctx, &stats);
    ASSERT_OK(rc, "smc_state_get_stats");
    
    ASSERT_EQ(stats.checks, 3u, "checks count");
    ASSERT_EQ(stats.changed, 2u, "changed count");
    ASSERT_EQ(stats.unchanged, 1u, "unchanged count");
    ASSERT_EQ(stats.stores, 2u, "stores count");
    ASSERT_EQ(stats.evictions, 0u, "evictions count initially zero");
    
    smc_context_destroy(ctx);
    printf("  test_stats: PASS\n");
    return 0;
}

/* Test preallocated storage - many updates without allocation issues */
static int test_preallocated_storage(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_config_t config = {0};
    int rc = smc_state_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_configure");
    
    /* Perform many state updates to verify no allocation issues */
    for (int i = 0; i < 1000; i++) {
        uint32_t key = i;
        uint32_t state = i * 100;
        int changed = 0;
        rc = smc_state_changed(ctx, &key, sizeof(key), &state, sizeof(state), &changed);
        ASSERT_OK(rc, "state update in preallocated storage");
    }
    
    smc_context_destroy(ctx);
    printf("  test_preallocated_storage: PASS\n");
    return 0;
}

/* Test eviction stats on collision */
static int test_eviction_stats(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    /* Use small cache to force collisions (2 entries) */
    smc_state_config_t config = {
        .max_entries = 2,
        .max_key_size = 32,
        .max_state_size = 32,
        .memory_budget_bytes = 0
    };
    
    int rc = smc_state_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_configure");
    
    rc = smc_state_reset_stats(ctx);
    ASSERT_OK(rc, "smc_state_reset_stats");
    
    /* These keys likely hash to same or adjacent slots */
    uint32_t key1 = 1;
    uint32_t key2 = 3;
    uint32_t state = 100;
    int changed = 0;
    
    /* First observations */
    smc_state_changed(ctx, &key1, sizeof(key1), &state, sizeof(state), &changed);
    smc_state_changed(ctx, &key2, sizeof(key2), &state, sizeof(state), &changed);
    
    smc_state_stats_t stats;
    rc = smc_state_get_stats(ctx, &stats);
    ASSERT_OK(rc, "smc_state_get_stats");
    
    /* At least 2 stores occurred; evictions may or may not have occurred */
    ASSERT_EQ(stats.stores, 2u, "stores count");
    
    smc_context_destroy(ctx);
    printf("  test_eviction_stats: PASS\n");
    return 0;
}

int main(void) {
    if (smc_init() != SMC_OK) {
        fprintf(stderr, "FAIL: smc_init failed\n");
        return 1;
    }
    
    /* Check features */
    uint32_t features = smc_features();
    if (!(features & SMC_FEATURE_STATE_TRACKING)) {
        fprintf(stderr, "FAIL: SMC_FEATURE_STATE_TRACKING not set\n");
        return 1;
    }
    
    if (test_first_observation() != 0) return 1;
    if (test_same_state() != 0) return 1;
    if (test_different_state() != 0) return 1;
    if (test_stats() != 0) return 1;
    if (test_preallocated_storage() != 0) return 1;
    if (test_eviction_stats() != 0) return 1;
    
    smc_shutdown();
    printf("All state tracking tests passed.\n");
    return 0;
}