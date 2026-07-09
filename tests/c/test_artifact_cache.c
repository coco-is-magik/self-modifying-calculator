/* tests/c/test_artifact_cache.c — acceptance test for artifact cache API
 *
 * Tests basic lookup/store/remove/clear operations and stats tracking.
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

/* Test basic lookup and store */
static int test_basic_operations(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) {
        fprintf(stderr, "FAIL: smc_context_create returned NULL\n");
        return 1;
    }
    
    smc_artifact_config_t config = {
        .max_entries = 16,
        .max_key_size = 32,
        .max_value_size = 256,
        .memory_budget_bytes = 0
    };
    
    int rc = smc_artifact_configure(ctx, &config);
    ASSERT_OK(rc, "smc_artifact_configure");
    
    /* Test miss before store */
    uint32_t key = 42;
    uint32_t value = 0xDEADBEEF;
    uint32_t out_value = 0;
    size_t out_size = 0;
    
    rc = smc_artifact_lookup(ctx, &key, sizeof(key), &out_value, sizeof(out_value), &out_size);
    ASSERT_EQ(rc, SMC_ERR_NOT_FOUND, "lookup miss before store");
    
    /* Store and lookup */
    rc = smc_artifact_store(ctx, &key, sizeof(key), &value, sizeof(value));
    ASSERT_OK(rc, "smc_artifact_store");
    
    rc = smc_artifact_lookup(ctx, &key, sizeof(key), &out_value, sizeof(out_value), &out_size);
    ASSERT_OK(rc, "lookup after store");
    ASSERT_EQ(out_value, value, "stored value matches");
    ASSERT_EQ(out_size, sizeof(value), "output size correct");
    
    smc_context_destroy(ctx);
    printf("  test_basic_operations: PASS\n");
    return 0;
}

/* Test stats counters */
static int test_stats(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) {
        fprintf(stderr, "FAIL: smc_context_create returned NULL\n");
        return 1;
    }
    
    smc_artifact_config_t config = {
        .max_entries = 16,
        .max_key_size = 32,
        .max_value_size = 256,
        .memory_budget_bytes = 0
    };
    
    int rc = smc_artifact_configure(ctx, &config);
    ASSERT_OK(rc, "smc_artifact_configure");
    
    rc = smc_artifact_reset_stats(ctx);
    ASSERT_OK(rc, "smc_artifact_reset_stats");
    
    /* Trigger some stats */
    uint32_t key = 1;
    uint32_t value = 100;
    uint32_t out_value = 0;
    
    smc_artifact_lookup(ctx, &key, sizeof(key), &out_value, sizeof(out_value), NULL); /* miss */
    smc_artifact_store(ctx, &key, sizeof(key), &value, sizeof(value));
    smc_artifact_lookup(ctx, &key, sizeof(key), &out_value, sizeof(out_value), NULL); /* hit */
    
    smc_artifact_stats_t stats;
    rc = smc_artifact_get_stats(ctx, &stats);
    ASSERT_OK(rc, "smc_artifact_get_stats");
    
    ASSERT_EQ(stats.lookups, 2u, "lookups count");
    ASSERT_EQ(stats.hits, 1u, "hits count");
    ASSERT_EQ(stats.misses, 1u, "misses count");
    ASSERT_EQ(stats.stores, 1u, "stores count");
    
    smc_context_destroy(ctx);
    printf("  test_stats: PASS\n");
    return 0;
}

/* Test zero-size key rejection */
static int test_zero_key(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_artifact_config_t config = {0};
    int rc = smc_artifact_configure(ctx, &config);
    ASSERT_OK(rc, "smc_artifact_configure");
    
    uint32_t value = 42;
    rc = smc_artifact_store(ctx, &value, 0, &value, sizeof(value));
    ASSERT_EQ(rc, SMC_ERR_INVALID, "zero-size key rejected");
    
    smc_context_destroy(ctx);
    printf("  test_zero_key: PASS\n");
    return 0;
}

/* Test oversized key/value */
static int test_size_limits(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_artifact_config_t config = {
        .max_key_size = 8,
        .max_value_size = 8,
        .memory_budget_bytes = 0
    };
    
    int rc = smc_artifact_configure(ctx, &config);
    ASSERT_OK(rc, "smc_artifact_configure");
    
    uint32_t small_key = 1, small_val = 1;
    uint8_t large_key[16], large_val[16];
    
    rc = smc_artifact_store(ctx, &small_key, sizeof(small_key), large_val, sizeof(large_val));
    ASSERT_EQ(rc, SMC_ERR_SIZE, "oversized value rejected");
    
    rc = smc_artifact_store(ctx, large_key, sizeof(large_key), &small_val, sizeof(small_val));
    ASSERT_EQ(rc, SMC_ERR_SIZE, "oversized key rejected");
    
    smc_context_destroy(ctx);
    printf("  test_size_limits: PASS\n");
    return 0;
}

/* Test double configure rejection */
static int test_double_configure(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_artifact_config_t config = {0};
    int rc = smc_artifact_configure(ctx, &config);
    ASSERT_OK(rc, "first smc_artifact_configure");
    
    rc = smc_artifact_configure(ctx, &config);
    ASSERT_EQ(rc, SMC_ERR_INVALID, "second configure rejected");
    
    smc_context_destroy(ctx);
    printf("  test_double_configure: PASS\n");
    return 0;
}

int main(void) {
    if (smc_init() != SMC_OK) {
        fprintf(stderr, "FAIL: smc_init failed\n");
        return 1;
    }
    
    /* Check features */
    uint32_t features = smc_features();
    if (!(features & SMC_FEATURE_ARTIFACT_CACHE)) {
        fprintf(stderr, "FAIL: SMC_FEATURE_ARTIFACT_CACHE not set\n");
        return 1;
    }
    
    if (test_basic_operations() != 0) return 1;
    if (test_stats() != 0) return 1;
    if (test_zero_key() != 0) return 1;
    if (test_size_limits() != 0) return 1;
    if (test_double_configure() != 0) return 1;
    
    smc_shutdown();
    printf("All artifact cache tests passed.\n");
    return 0;
}