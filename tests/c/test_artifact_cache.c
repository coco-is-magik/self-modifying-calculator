/* tests/c/test_artifact_cache.c — acceptance test for artifact cache API
 *
 * Tests basic lookup/store/remove/clear operations and preallocated storage.
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
    
    uint32_t key = 42;
    uint32_t value = 0xDEADBEEF;
    uint32_t out_value = 0;
    size_t out_size = 0;
    
    rc = smc_artifact_lookup(ctx, &key, sizeof(key), &out_value, sizeof(out_value), &out_size);
    ASSERT_EQ(rc, SMC_ERR_NOT_FOUND, "lookup miss before store");
    
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
    if (!ctx) return 1;
    
    smc_artifact_config_t config = {0};
    int rc = smc_artifact_configure(ctx, &config);
    ASSERT_OK(rc, "smc_artifact_configure");
    
    rc = smc_artifact_reset_stats(ctx);
    ASSERT_OK(rc, "smc_artifact_reset_stats");
    
    uint32_t key = 1, value = 100;
    uint32_t out_value = 0;
    
    smc_artifact_lookup(ctx, &key, sizeof(key), &out_value, sizeof(out_value), NULL);
    smc_artifact_store(ctx, &key, sizeof(key), &value, sizeof(value));
    smc_artifact_lookup(ctx, &key, sizeof(key), &out_value, sizeof(out_value), NULL);
    
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
    
    uint32_t small_key = 1;
    uint8_t small_val = 1;
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

/* Test buffer-too-small handling */
static int test_buffer_too_small(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_artifact_config_t config = {0};
    int rc = smc_artifact_configure(ctx, &config);
    ASSERT_OK(rc, "smc_artifact_configure");
    
    uint32_t key = 42, value = 0xDEADBEEF;
    uint8_t small_buffer[2];
    size_t out_size = 0;
    
    rc = smc_artifact_store(ctx, &key, sizeof(key), &value, sizeof(value));
    ASSERT_OK(rc, "smc_artifact_store");
    
    rc = smc_artifact_lookup(ctx, &key, sizeof(key), small_buffer, sizeof(small_buffer), &out_size);
    ASSERT_EQ(rc, SMC_ERR_SIZE, "buffer-too-small returns SMC_ERR_SIZE");
    ASSERT_EQ((uint32_t)out_size, sizeof(value), "out_size reports needed size");
    
    smc_context_destroy(ctx);
    printf("  test_buffer_too_small: PASS\n");
    return 0;
}

/* Test collision eviction with stat verification */
static int test_collision_eviction(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_artifact_config_t config = {
        .max_entries = 2,
        .max_key_size = 32,
        .max_value_size = 32,
        .memory_budget_bytes = 0
    };
    
    int rc = smc_artifact_configure(ctx, &config);
    ASSERT_OK(rc, "smc_artifact_configure");
    
    rc = smc_artifact_reset_stats(ctx);
    ASSERT_OK(rc, "smc_artifact_reset_stats");
    
    uint32_t key1 = 1, key2 = 3;
    uint32_t value1 = 100, value2 = 200;
    
    rc = smc_artifact_store(ctx, &key1, sizeof(key1), &value1, sizeof(value1));
    ASSERT_OK(rc, "store key1");
    
    rc = smc_artifact_store(ctx, &key2, sizeof(key2), &value2, sizeof(value2));
    ASSERT_OK(rc, "store key2 (may evict key1)");
    
    smc_artifact_stats_t stats;
    rc = smc_artifact_get_stats(ctx, &stats);
    ASSERT_OK(rc, "smc_artifact_get_stats");
    
    ASSERT_EQ(stats.stores, 2u, "stores count");
    
    smc_context_destroy(ctx);
    printf("  test_collision_eviction: PASS\n");
    return 0;
}

/* Test remove operation and stats */
static int test_remove_operation(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_artifact_config_t config = {0};
    int rc = smc_artifact_configure(ctx, &config);
    ASSERT_OK(rc, "smc_artifact_configure");
    
    rc = smc_artifact_reset_stats(ctx);
    ASSERT_OK(rc, "smc_artifact_reset_stats");
    
    uint32_t key = 42;
    uint8_t value[] = "hello";
    
    rc = smc_artifact_store(ctx, &key, sizeof(key), value, sizeof(value));
    ASSERT_OK(rc, "store");
    
    rc = smc_artifact_remove(ctx, &key, sizeof(key));
    ASSERT_OK(rc, "remove");
    
    smc_artifact_stats_t stats;
    rc = smc_artifact_get_stats(ctx, &stats);
    ASSERT_OK(rc, "smc_artifact_get_stats");
    
    ASSERT_EQ(stats.removes, 1u, "removes count");
    
    smc_context_destroy(ctx);
    printf("  test_remove_operation: PASS\n");
    return 0;
}

/* Test zero-size value handling */
static int test_zero_size_value(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_artifact_config_t config = {0};
    int rc = smc_artifact_configure(ctx, &config);
    ASSERT_OK(rc, "smc_artifact_configure");
    
    uint32_t key = 42;
    rc = smc_artifact_store(ctx, &key, sizeof(key), &key, 0);
    ASSERT_OK(rc, "zero-size value store");
    
    uint32_t out = 0;
    size_t out_size = 999;
    rc = smc_artifact_lookup(ctx, &key, sizeof(key), &out, sizeof(out), &out_size);
    ASSERT_OK(rc, "lookup zero-size value");
    ASSERT_EQ(out_size, 0u, "out_size is 0 for zero-size value");
    
    smc_context_destroy(ctx);
    printf("  test_zero_size_value: PASS\n");
    return 0;
}

/* Test updates counter on same-key store */
static int test_updates_counter(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_artifact_config_t config = {0};
    int rc = smc_artifact_configure(ctx, &config);
    ASSERT_OK(rc, "smc_artifact_configure");
    
    rc = smc_artifact_reset_stats(ctx);
    ASSERT_OK(rc, "smc_artifact_reset_stats");
    
    uint32_t key = 1;
    uint32_t value1 = 100, value2 = 200;
    
    rc = smc_artifact_store(ctx, &key, sizeof(key), &value1, sizeof(value1));
    ASSERT_OK(rc, "first store");
    
    rc = smc_artifact_store(ctx, &key, sizeof(key), &value2, sizeof(value2));
    ASSERT_OK(rc, "second store same key");
    
    smc_artifact_stats_t stats;
    rc = smc_artifact_get_stats(ctx, &stats);
    ASSERT_OK(rc, "smc_artifact_get_stats");
    
    ASSERT_EQ(stats.stores, 2u, "total stores count");
    ASSERT_EQ(stats.updates, 1u, "updates count (same key replacement)");
    
    smc_context_destroy(ctx);
    printf("  test_updates_counter: PASS\n");
    return 0;
}

/* Test preallocated storage (v2.1) - no malloc per store */
static int test_preallocated_storage(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_artifact_config_t config = {0};
    int rc = smc_artifact_configure(ctx, &config);
    ASSERT_OK(rc, "smc_artifact_configure");
    
    /* Store and retrieve multiple artifacts - uses preallocated slots */
    for (int i = 0; i < 100; i++) {
        uint32_t key = i;
        uint32_t value = i * 1000;
        rc = smc_artifact_store(ctx, &key, sizeof(key), &value, sizeof(value));
        ASSERT_OK(rc, "store in preallocated storage");
    }
    
    /* Verify all stored correctly */
    for (int i = 0; i < 100; i++) {
        uint32_t key = i;
        uint32_t out_value = 0;
        size_t out_size = 0;
        rc = smc_artifact_lookup(ctx, &key, sizeof(key), &out_value, sizeof(out_value), &out_size);
        ASSERT_OK(rc, "lookup");
        ASSERT_EQ(out_value, (uint32_t)(i * 1000), "value correct");
    }
    
    smc_context_destroy(ctx);
    printf("  test_preallocated_storage: PASS\n");
    return 0;
}

/* Test memory budget enforcement - tiny budget should be rejected */
static int test_memory_budget_enforcement(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    /* Configure with reasonable sizes but tiny budget */
    smc_artifact_config_t config = {
        .max_entries = 16,
        .max_key_size = 32,
        .max_value_size = 256,
        .memory_budget_bytes = 10  /* Way too small */
    };
    
    int rc = smc_artifact_configure(ctx, &config);
    ASSERT_EQ(rc, SMC_ERR_CAPACITY, "memory budget rejection");
    
    smc_context_destroy(ctx);
    printf("  test_memory_budget_enforcement: PASS\n");
    return 0;
}

/* Test memory budget respected when within limit */
static int test_memory_budget_respected(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    /* Configure with adequate budget - should succeed */
    smc_artifact_config_t config = {
        .max_entries = 16,
        .max_key_size = 32,
        .max_value_size = 256,
        .memory_budget_bytes = 65536  /* 64KB - plenty of room */
    };
    
    int rc = smc_artifact_configure(ctx, &config);
    ASSERT_OK(rc, "configure with adequate budget");
    
    smc_artifact_stats_t stats;
    rc = smc_artifact_get_stats(ctx, &stats);
    ASSERT_OK(rc, "get stats");
    
    smc_context_destroy(ctx);
    printf("  test_memory_budget_respected: PASS\n");
    return 0;
}

int main(void) {
    if (smc_init() != SMC_OK) {
        fprintf(stderr, "FAIL: smc_init failed\n");
        return 1;
    }
    
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
    if (test_buffer_too_small() != 0) return 1;
    if (test_collision_eviction() != 0) return 1;
    if (test_zero_size_value() != 0) return 1;
    if (test_updates_counter() != 0) return 1;
    if (test_remove_operation() != 0) return 1;
    if (test_preallocated_storage() != 0) return 1;
    if (test_memory_budget_enforcement() != 0) return 1;
    if (test_memory_budget_respected() != 0) return 1;
    
    smc_shutdown();
    printf("All artifact cache tests passed.\n");
    return 0;
}
