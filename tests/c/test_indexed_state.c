/* tests/c/test_indexed_state.c — acceptance tests for indexed state API
 *
 * Tests indexed state_changed_index() and smc_state_diff_indexed_batch().
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
static int test_indexed_first_observation(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 4,  /* 4 bytes to match uint32_t */
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    uint32_t state = 0xDEADBEEF;
    int changed = 0;
    
    rc = smc_state_changed_index(ctx, 42, &state, sizeof(state), &changed);
    ASSERT_OK(rc, "smc_state_changed_index");
    ASSERT_EQ(changed, 1, "first observation is changed");
    
    smc_context_destroy(ctx);
    printf("  test_indexed_first_observation: PASS\n");
    return 0;
}

/* Test same state returns unchanged */
static int test_indexed_same_state(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 4,  /* 4 bytes to match uint32_t */
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    uint32_t state = 0xDEADBEEF;
    int changed = 0;
    
    rc = smc_state_changed_index(ctx, 42, &state, sizeof(state), &changed);
    ASSERT_OK(rc, "first check");
    ASSERT_EQ(changed, 1, "first is changed");
    
    changed = 0;
    rc = smc_state_changed_index(ctx, 42, &state, sizeof(state), &changed);
    ASSERT_OK(rc, "second check same state");
    ASSERT_EQ(changed, 0, "same state is unchanged");
    
    smc_context_destroy(ctx);
    printf("  test_indexed_same_state: PASS\n");
    return 0;
}

/* Test different state returns changed */
static int test_indexed_different_state(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 4,  /* 4 bytes to match uint32_t */
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    uint32_t state1 = 100;
    uint32_t state2 = 200;
    int changed = 0;
    
    rc = smc_state_changed_index(ctx, 42, &state1, sizeof(state1), &changed);
    ASSERT_OK(rc, "first check");
    ASSERT_EQ(changed, 1, "first is changed");
    
    changed = 0;
    rc = smc_state_changed_index(ctx, 42, &state2, sizeof(state2), &changed);
    ASSERT_OK(rc, "second check different state");
    ASSERT_EQ(changed, 1, "different state is changed");
    
    smc_context_destroy(ctx);
    printf("  test_indexed_different_state: PASS\n");
    return 0;
}

/* Test zero-size state */
static int test_indexed_zero_size_state(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 0,  /* zero-size state */
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    int changed = 0;
    
    /* First observation - should be changed */
    rc = smc_state_changed_index(ctx, 42, NULL, 0, &changed);
    ASSERT_OK(rc, "first observation zero-size");
    ASSERT_EQ(changed, 1, "zero-size first observation is changed");
    
    /* Second observation - should be unchanged */
    changed = 0;
    rc = smc_state_changed_index(ctx, 42, NULL, 0, &changed);
    ASSERT_OK(rc, "second observation zero-size");
    ASSERT_EQ(changed, 0, "zero-size second observation is unchanged");
    
    smc_context_destroy(ctx);
    printf("  test_indexed_zero_size_state: PASS\n");
    return 0;
}

/* Test out-of-range index */
static int test_indexed_out_of_range(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 4,  /* 4 bytes to match uint32_t */
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    rc = smc_state_indexed_reset_stats(ctx);
    ASSERT_OK(rc, "smc_state_indexed_reset_stats");
    
    uint32_t state = 100;
    int changed = 0;
    
    /* Index 100 is out of range (valid: 0-99) */
    rc = smc_state_changed_index(ctx, 100, &state, sizeof(state), &changed);
    if (rc != SMC_ERR_SIZE) {
        fprintf(stderr, "FAIL: out of range should return SMC_ERR_SIZE, got %d\n", rc);
        return 1;
    }
    
    smc_state_indexed_stats_t stats;
    rc = smc_state_indexed_get_stats(ctx, &stats);
    ASSERT_OK(rc, "smc_state_indexed_get_stats");
    ASSERT_EQ(stats.out_of_range, 1u, "out_of_range stat");
    
    smc_context_destroy(ctx);
    printf("  test_indexed_out_of_range: PASS\n");
    return 0;
}

/* Test wrong state size */
static int test_indexed_wrong_size(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 8,  /* 8 bytes */
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    uint32_t state = 100;
    int changed = 0;
    
    /* Pass wrong state size (4 instead of 8) */
    rc = smc_state_changed_index(ctx, 42, &state, 4, &changed);
    if (rc != SMC_ERR_SIZE) {
        fprintf(stderr, "FAIL: wrong size should return SMC_ERR_SIZE, got %d\n", rc);
        return 1;
    }
    
    smc_context_destroy(ctx);
    printf("  test_indexed_wrong_size: PASS\n");
    return 0;
}

/* Test clear resets validity */
static int test_indexed_clear(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 4,  /* 4 bytes to match uint32_t */
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    rc = smc_state_indexed_reset_stats(ctx);
    ASSERT_OK(rc, "smc_state_indexed_reset_stats");
    
    uint32_t state = 100;
    int changed = 0;
    
    /* First observation */
    rc = smc_state_changed_index(ctx, 42, &state, sizeof(state), &changed);
    ASSERT_OK(rc, "first observation");
    ASSERT_EQ(changed, 1, "first is changed");
    
    /* Clear */
    rc = smc_state_indexed_clear(ctx);
    ASSERT_OK(rc, "smc_state_indexed_clear");
    
    smc_state_indexed_stats_t stats;
    rc = smc_state_indexed_get_stats(ctx, &stats);
    ASSERT_OK(rc, "smc_state_indexed_get_stats");
    ASSERT_EQ(stats.clears, 1u, "clears stat");
    
    /* After clear, should be changed again */
    changed = 0;
    rc = smc_state_changed_index(ctx, 42, &state, sizeof(state), &changed);
    ASSERT_OK(rc, "after clear observation");
    ASSERT_EQ(changed, 1, "after clear is changed");
    
    smc_context_destroy(ctx);
    printf("  test_indexed_clear: PASS\n");
    return 0;
}

/* Test stats counters */
static int test_indexed_stats(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 4,  /* 4 bytes to match uint32_t */
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    rc = smc_state_indexed_reset_stats(ctx);
    ASSERT_OK(rc, "smc_state_indexed_reset_stats");
    
    uint32_t state = 100;
    int changed = 0;
    
    /* First observation - changed */
    smc_state_changed_index(ctx, 42, &state, sizeof(state), &changed);
    /* Same state - unchanged */
    smc_state_changed_index(ctx, 42, &state, sizeof(state), &changed);
    /* Different state - changed */
    uint32_t state2 = 200;
    smc_state_changed_index(ctx, 42, &state2, sizeof(state2), &changed);
    
    smc_state_indexed_stats_t stats;
    rc = smc_state_indexed_get_stats(ctx, &stats);
    ASSERT_OK(rc, "smc_state_indexed_get_stats");
    
    ASSERT_EQ(stats.checks, 3u, "checks count");
    ASSERT_EQ(stats.changed, 2u, "changed count");
    ASSERT_EQ(stats.unchanged, 1u, "unchanged count");
    ASSERT_EQ(stats.stores, 2u, "stores count");
    ASSERT_EQ(stats.bytes_compared, 12u, "bytes_compared count");
    
    smc_context_destroy(ctx);
    printf("  test_indexed_stats: PASS\n");
    return 0;
}

/* Test memory budget enforcement */
static int test_indexed_memory_budget(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 1000,
        .state_size = 1000,
        .memory_budget_bytes = 100  /* Budget too small */
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    if (rc != SMC_ERR_CAPACITY) {
        fprintf(stderr, "FAIL: small budget should return SMC_ERR_CAPACITY, got %d\n", rc);
        return 1;
    }
    
    smc_context_destroy(ctx);
    printf("  test_indexed_memory_budget: PASS\n");
    return 0;
}

/* Test configure-after-operation error */
static int test_indexed_double_configure(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 4,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "first configure");
    
    /* Second configure should fail */
    rc = smc_state_indexed_configure(ctx, &config);
    if (rc != SMC_ERR_INVALID) {
        fprintf(stderr, "FAIL: second configure should return SMC_ERR_INVALID, got %d\n", rc);
        return 1;
    }
    
    smc_context_destroy(ctx);
    printf("  test_indexed_double_configure: PASS\n");
    return 0;
}

/* Test batch: all-new returns all dirty indices */
static int test_batch_all_new(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 4,  /* 4 bytes to match uint32_t */
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    rc = smc_state_indexed_reset_stats(ctx);
    ASSERT_OK(rc, "smc_state_indexed_reset_stats");
    
    uint32_t states[100];
    for (int i = 0; i < 100; i++) {
        states[i] = i * 1000;
    }
    
    uint32_t dirty_indices[100];
    size_t dirty_count = 0;
    
    rc = smc_state_diff_indexed_batch(ctx, states, 100, 4, dirty_indices, 100, &dirty_count);
    ASSERT_OK(rc, "smc_state_diff_indexed_batch");
    ASSERT_EQ(dirty_count, 100u, "all 100 dirty");
    
    /* Verify all indices are present */
    for (size_t i = 0; i < 100; i++) {
        if (dirty_indices[i] != (uint32_t)i) {
            fprintf(stderr, "FAIL: dirty_indices[%zu] = %u, expected %zu\n", i, dirty_indices[i], i);
            return 1;
        }
    }
    
    smc_context_destroy(ctx);
    printf("  test_batch_all_new: PASS\n");
    return 0;
}

/* Test batch: same batch returns zero dirty indices */
static int test_batch_same_batch(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 4,  /* 4 bytes to match uint32_t */
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    uint32_t states[100];
    for (int i = 0; i < 100; i++) {
        states[i] = i * 1000;
    }
    
    /* First call - all dirty */
    uint32_t dirty_indices[100];
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_batch(ctx, states, 100, 4, dirty_indices, 100, &dirty_count);
    ASSERT_OK(rc, "first batch");
    
    /* Second call - all unchanged */
    dirty_count = 0;
    rc = smc_state_diff_indexed_batch(ctx, states, 100, 4, dirty_indices, 100, &dirty_count);
    ASSERT_OK(rc, "second batch");
    ASSERT_EQ(dirty_count, 0u, "no dirty on second batch");
    
    smc_context_destroy(ctx);
    printf("  test_batch_same_batch: PASS\n");
    return 0;
}

/* Test batch: one changed record */
static int test_batch_one_changed(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 4,  /* 4 bytes to match uint32_t */
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    uint32_t states[100];
    for (int i = 0; i < 100; i++) {
        states[i] = i * 1000;
    }
    
    /* First call - all dirty */
    uint32_t dirty_indices[100];
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_batch(ctx, states, 100, 4, dirty_indices, 100, &dirty_count);
    ASSERT_OK(rc, "first batch");
    
    /* Modify one state */
    states[50] = 999;
    
    /* Second call - one dirty at index 50 */
    dirty_count = 0;
    rc = smc_state_diff_indexed_batch(ctx, states, 100, 4, dirty_indices, 100, &dirty_count);
    ASSERT_OK(rc, "second batch");
    ASSERT_EQ(dirty_count, 1u, "one dirty index");
    ASSERT_EQ(dirty_indices[0], 50u, "dirty index is 50");
    
    smc_context_destroy(ctx);
    printf("  test_batch_one_changed: PASS\n");
    return 0;
}

/* Test batch: NULL dirty_indices with zero capacity */
static int test_batch_null_dirty_no_write(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 4,  /* 4 bytes to match uint32_t */
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    rc = smc_state_indexed_reset_stats(ctx);
    ASSERT_OK(rc, "smc_state_indexed_reset_stats");
    
    uint32_t states[100] = {0};
    size_t dirty_count = 0;
    
    /* NULL dirty_indices with capacity 0 - should just count */
    rc = smc_state_diff_indexed_batch(ctx, states, 100, 4, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "batch with NULL dirty_indices");
    ASSERT_EQ(dirty_count, 100u, "counted all as changed");
    
    smc_state_indexed_stats_t stats;
    rc = smc_state_indexed_get_stats(ctx, &stats);
    ASSERT_OK(rc, "smc_state_indexed_get_stats");
    ASSERT_EQ(stats.checks, 100u, "stats checks");
    
    smc_context_destroy(ctx);
    printf("  test_batch_null_dirty_no_write: PASS\n");
    return 0;
}

/* Test batch: NULL dirty_indices with non-zero capacity error */
static int test_batch_null_dirty_nonzero_capacity(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 4,  /* 4 bytes to match uint32_t */
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    uint32_t states[100] = {0};
    size_t dirty_count = 0;
    
    /* NULL dirty_indices with capacity > 0 - should error */
    rc = smc_state_diff_indexed_batch(ctx, states, 100, 4, NULL, 100, &dirty_count);
    if (rc != SMC_ERR_INVALID) {
        fprintf(stderr, "FAIL: NULL dirty_indices with capacity > 0 should return SMC_ERR_INVALID, got %d\n", rc);
        return 1;
    }
    
    smc_context_destroy(ctx);
    printf("  test_batch_null_dirty_nonzero_capacity: PASS\n");
    return 0;
}

/* Test batch: count exceeds configured count */
static int test_batch_count_too_large(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 4,  /* 4 bytes to match uint32_t */
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    uint32_t states[200] = {0};
    uint32_t dirty_indices[200];
    size_t dirty_count = 0;
    
    /* count > configured count should error */
    rc = smc_state_diff_indexed_batch(ctx, states, 200, 4, dirty_indices, 200, &dirty_count);
    if (rc != SMC_ERR_SIZE) {
        fprintf(stderr, "FAIL: count > config count should return SMC_ERR_SIZE, got %d\n", rc);
        return 1;
    }
    
    smc_context_destroy(ctx);
    printf("  test_batch_count_too_large: PASS\n");
    return 0;
}

/* Test batch: stride larger than state size */
static int test_batch_stride_larger(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 4,  /* 4 bytes to match uint32_t */
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    /* Strided array: each uint32_t followed by 4 bytes padding */
    uint8_t states[12 * 100]; /* 4 bytes state + 8 padding (stride 12) */
    for (int i = 0; i < 100; i++) {
        uint32_t *state = (uint32_t *)(states + i * 12);
        *state = i * 1000;
    }
    
    uint32_t dirty_indices[100];
    size_t dirty_count = 0;
    
    rc = smc_state_diff_indexed_batch(ctx, states, 100, 12, dirty_indices, 100, &dirty_count);
    ASSERT_OK(rc, "batch with stride");
    ASSERT_EQ(dirty_count, 100u, "all 100 dirty with stride");
    
    smc_context_destroy(ctx);
    printf("  test_batch_stride_larger: PASS\n");
    return 0;
}

/* Test batch: count == 0 with NULL states is valid */
static int test_batch_count_zero(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 4,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    rc = smc_state_indexed_reset_stats(ctx);
    ASSERT_OK(rc, "smc_state_indexed_reset_stats");
    
    size_t dirty_count = 12345; /* sentinel value */
    rc = smc_state_diff_indexed_batch(ctx, NULL, 0, 4, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "batch with count=0");
    ASSERT_EQ(dirty_count, 0u, "dirty_count is 0 for count=0");
    
    smc_state_indexed_stats_t stats;
    rc = smc_state_indexed_get_stats(ctx, &stats);
    ASSERT_OK(rc, "smc_state_indexed_get_stats");
    ASSERT_EQ(stats.checks, 0u, "no checks for count=0");
    ASSERT_EQ(stats.changed, 0u, "no changes for count=0");
    ASSERT_EQ(stats.unchanged, 0u, "no unchanged for count=0");
    
    smc_context_destroy(ctx);
    printf("  test_batch_count_zero: PASS\n");
    return 0;
}

/* Test batch: stride == 0 returns SMC_ERR_INVALID */
static int test_batch_zero_stride(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 4,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    uint32_t states[100] = {0};
    uint32_t dirty_indices[100];
    size_t dirty_count = 0;
    
    rc = smc_state_diff_indexed_batch(ctx, states, 100, 0, dirty_indices, 100, &dirty_count);
    if (rc != SMC_ERR_INVALID) {
        fprintf(stderr, "FAIL: stride=0 should return SMC_ERR_INVALID, got %d\n", rc);
        return 1;
    }
    
    smc_context_destroy(ctx);
    printf("  test_batch_zero_stride: PASS\n");
    return 0;
}

/* Test batch: states == NULL with state_size > 0 returns SMC_ERR_INVALID */
static int test_batch_states_null_with_size(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 4,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    uint32_t dirty_indices[100];
    size_t dirty_count = 0;
    
    rc = smc_state_diff_indexed_batch(ctx, NULL, 100, 4, dirty_indices, 100, &dirty_count);
    if (rc != SMC_ERR_INVALID) {
        fprintf(stderr, "FAIL: NULL states with state_size>0 should return SMC_ERR_INVALID, got %d\n", rc);
        return 1;
    }
    
    smc_context_destroy(ctx);
    printf("  test_batch_states_null_with_size: PASS\n");
    return 0;
}

/* Test batch: states == NULL with state_size == 0 is valid */
static int test_batch_states_null_zero_size(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 0,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_batch(ctx, NULL, 100, 1, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "batch with NULL states and state_size=0");
    ASSERT_EQ(dirty_count, 100u, "all zero-size records are changed on first observation");
    
    smc_context_destroy(ctx);
    printf("  test_batch_states_null_zero_size: PASS\n");
    return 0;
}

/* Test batch: stride < state_size returns SMC_ERR_SIZE */
static int test_batch_stride_too_small(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 8,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    uint8_t states[100 * 4]; /* stride 4 < state_size 8 */
    uint32_t dirty_indices[100];
    size_t dirty_count = 0;
    
    rc = smc_state_diff_indexed_batch(ctx, states, 100, 4, dirty_indices, 100, &dirty_count);
    if (rc != SMC_ERR_SIZE) {
        fprintf(stderr, "FAIL: stride < state_size should return SMC_ERR_SIZE, got %d\n", rc);
        return 1;
    }
    
    smc_context_destroy(ctx);
    printf("  test_batch_stride_too_small: PASS\n");
    return 0;
}

/* Test batch: dirty_capacity smaller than dirty count */
static int test_batch_dirty_capacity_overflow(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 4,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    rc = smc_state_indexed_reset_stats(ctx);
    ASSERT_OK(rc, "smc_state_indexed_reset_stats");
    
    uint32_t states[100];
    for (int i = 0; i < 100; i++) {
        states[i] = (uint32_t)i;
    }
    
    uint32_t dirty_indices[5] = {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
    size_t dirty_count = 0;
    
    /* First batch: all 100 changed, but only 5 indices written */
    rc = smc_state_diff_indexed_batch(ctx, states, 100, 4, dirty_indices, 5, &dirty_count);
    ASSERT_OK(rc, "batch with capacity overflow");
    ASSERT_EQ(dirty_count, 100u, "full dirty count reported");
    
    for (size_t i = 0; i < 5; i++) {
        ASSERT_EQ(dirty_indices[i], (uint32_t)i, "partial indices written in order");
    }
    
    smc_state_indexed_stats_t stats;
    rc = smc_state_indexed_get_stats(ctx, &stats);
    ASSERT_OK(rc, "smc_state_indexed_get_stats");
    ASSERT_EQ(stats.checks, 100u, "checks count after first batch");
    ASSERT_EQ(stats.changed, 100u, "changed count after first batch");
    ASSERT_EQ(stats.unchanged, 0u, "unchanged count after first batch");
    ASSERT_EQ(stats.stores, 100u, "stores count after first batch");
    
    /* Second identical batch: state was stored for all 100, so all unchanged */
    dirty_count = 0;
    rc = smc_state_diff_indexed_batch(ctx, states, 100, 4, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "second batch with NULL dirty_indices");
    ASSERT_EQ(dirty_count, 0u, "no dirty on second batch");
    
    rc = smc_state_indexed_get_stats(ctx, &stats);
    ASSERT_OK(rc, "smc_state_indexed_get_stats");
    ASSERT_EQ(stats.checks, 200u, "checks count after second batch");
    ASSERT_EQ(stats.changed, 100u, "changed count unchanged after second batch");
    ASSERT_EQ(stats.unchanged, 100u, "unchanged count after second batch");
    ASSERT_EQ(stats.stores, 100u, "stores count unchanged after second batch");
    
    smc_context_destroy(ctx);
    printf("  test_batch_dirty_capacity_overflow: PASS\n");
    return 0;
}

/* Test batch: stats accumulate correctly across multiple identical calls */
static int test_batch_stats_accumulation(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 4,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    rc = smc_state_indexed_reset_stats(ctx);
    ASSERT_OK(rc, "smc_state_indexed_reset_stats");
    
    uint32_t states[100];
    for (int i = 0; i < 100; i++) {
        states[i] = (uint32_t)i;
    }
    
    size_t dirty_count = 0;
    
    /* First batch: all changed */
    rc = smc_state_diff_indexed_batch(ctx, states, 100, 4, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "first batch");
    ASSERT_EQ(dirty_count, 100u, "first batch all changed");
    
    /* Second batch: all unchanged */
    rc = smc_state_diff_indexed_batch(ctx, states, 100, 4, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "second batch");
    ASSERT_EQ(dirty_count, 0u, "second batch all unchanged");
    
    /* Third batch: all unchanged */
    rc = smc_state_diff_indexed_batch(ctx, states, 100, 4, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "third batch");
    ASSERT_EQ(dirty_count, 0u, "third batch all unchanged");
    
    smc_state_indexed_stats_t stats;
    rc = smc_state_indexed_get_stats(ctx, &stats);
    ASSERT_OK(rc, "smc_state_indexed_get_stats");
    ASSERT_EQ(stats.checks, 300u, "cumulative checks");
    ASSERT_EQ(stats.changed, 100u, "only first batch changed");
    ASSERT_EQ(stats.unchanged, 200u, "second and third batches unchanged");
    ASSERT_EQ(stats.stores, 100u, "only first batch stores");
    ASSERT_EQ(stats.bytes_compared, 1200u, "bytes compared total");
    
    smc_context_destroy(ctx);
    printf("  test_batch_stats_accumulation: PASS\n");
    return 0;
}

/* Test batch: multiple modified records return exact dirty indices in order */
static int test_batch_multiple_modified_ordered(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 4,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    uint32_t states[100];
    for (int i = 0; i < 100; i++) {
        states[i] = (uint32_t)i;
    }
    
    uint32_t dirty_indices[100];
    size_t dirty_count = 0;
    
    /* First batch: populate */
    rc = smc_state_diff_indexed_batch(ctx, states, 100, 4, dirty_indices, 100, &dirty_count);
    ASSERT_OK(rc, "first batch");
    ASSERT_EQ(dirty_count, 100u, "all dirty on first batch");
    
    /* Modify records 10, 20, 30 */
    states[10] = 1000;
    states[20] = 2000;
    states[30] = 3000;
    
    /* Second batch: exactly three dirty indices in order */
    rc = smc_state_diff_indexed_batch(ctx, states, 100, 4, dirty_indices, 100, &dirty_count);
    ASSERT_OK(rc, "second batch");
    ASSERT_EQ(dirty_count, 3u, "three dirty indices");
    ASSERT_EQ(dirty_indices[0], 10u, "first dirty index");
    ASSERT_EQ(dirty_indices[1], 20u, "second dirty index");
    ASSERT_EQ(dirty_indices[2], 30u, "third dirty index");
    
    smc_context_destroy(ctx);
    printf("  test_batch_multiple_modified_ordered: PASS\n");
    return 0;
}

/* Test batch: renderer-shaped 3-frame workload */
static int test_renderer_shaped_workload(void) {
    const size_t RECORD_COUNT = 41600;
    const size_t STATE_SIZE = 8;
    const size_t MODIFIED_INDEX = 12345;
    
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = RECORD_COUNT,
        .state_size = STATE_SIZE,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    rc = smc_state_indexed_reset_stats(ctx);
    ASSERT_OK(rc, "smc_state_indexed_reset_stats");
    
    uint8_t *states = (uint8_t *)malloc(RECORD_COUNT * STATE_SIZE);
    if (!states) {
        fprintf(stderr, "FAIL: out of memory for renderer states\n");
        return 1;
    }
    
    /* Initialize deterministic renderer-like state */
    for (size_t i = 0; i < RECORD_COUNT; i++) {
        uint64_t *cell = (uint64_t *)(states + i * STATE_SIZE);
        *cell = (uint64_t)i;
    }
    
    uint32_t *dirty_indices = (uint32_t *)malloc(RECORD_COUNT * sizeof(uint32_t));
    if (!dirty_indices) {
        free(states);
        fprintf(stderr, "FAIL: out of memory for dirty_indices\n");
        return 1;
    }
    
    size_t dirty_count = 0;
    smc_state_indexed_stats_t stats;
    
    /* Frame 1: all records observed for the first time */
    rc = smc_state_diff_indexed_batch(ctx, states, RECORD_COUNT, STATE_SIZE, dirty_indices, RECORD_COUNT, &dirty_count);
    ASSERT_OK(rc, "frame 1 batch");
    ASSERT_EQ(dirty_count, RECORD_COUNT, "frame 1 all records changed");
    
    rc = smc_state_indexed_get_stats(ctx, &stats);
    ASSERT_OK(rc, "frame 1 stats");
    ASSERT_EQ(stats.checks, RECORD_COUNT, "frame 1 checks");
    ASSERT_EQ(stats.changed, RECORD_COUNT, "frame 1 changed");
    ASSERT_EQ(stats.unchanged, 0u, "frame 1 unchanged");
    
    /* Frame 2: identical records, all unchanged */
    rc = smc_state_diff_indexed_batch(ctx, states, RECORD_COUNT, STATE_SIZE, dirty_indices, RECORD_COUNT, &dirty_count);
    ASSERT_OK(rc, "frame 2 batch");
    ASSERT_EQ(dirty_count, 0u, "frame 2 no records changed");
    
    rc = smc_state_indexed_get_stats(ctx, &stats);
    ASSERT_OK(rc, "frame 2 stats");
    ASSERT_EQ(stats.checks, RECORD_COUNT * 2, "frame 2 checks");
    ASSERT_EQ(stats.changed, RECORD_COUNT, "frame 2 changed unchanged");
    ASSERT_EQ(stats.unchanged, RECORD_COUNT, "frame 2 unchanged");
    
    /* Frame 3: exactly one record changed */
    uint64_t *modified_cell = (uint64_t *)(states + MODIFIED_INDEX * STATE_SIZE);
    *modified_cell = 0xDEADBEEF;
    
    rc = smc_state_diff_indexed_batch(ctx, states, RECORD_COUNT, STATE_SIZE, dirty_indices, RECORD_COUNT, &dirty_count);
    ASSERT_OK(rc, "frame 3 batch");
    ASSERT_EQ(dirty_count, 1u, "frame 3 one record changed");
    ASSERT_EQ(dirty_indices[0], (uint32_t)MODIFIED_INDEX, "frame 3 dirty index");
    
    rc = smc_state_indexed_get_stats(ctx, &stats);
    ASSERT_OK(rc, "frame 3 stats");
    ASSERT_EQ(stats.checks, RECORD_COUNT * 3, "frame 3 checks");
    ASSERT_EQ(stats.changed, RECORD_COUNT + 1, "frame 3 changed");
    ASSERT_EQ(stats.unchanged, (RECORD_COUNT * 2) - 1, "frame 3 unchanged");
    
    free(states);
    free(dirty_indices);
    smc_context_destroy(ctx);
    printf("  test_renderer_shaped_workload: PASS\n");
    return 0;
}

/* Test batch: detect accidental state reset between frames */
static int test_batch_no_accidental_reset(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 4,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    uint32_t states[100];
    for (int i = 0; i < 100; i++) {
        states[i] = (uint32_t)i;
    }
    
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_batch(ctx, states, 100, 4, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "first batch");
    ASSERT_EQ(dirty_count, 100u, "first batch all changed");
    
    /* Second identical batch should report zero changed. If the implementation
     * accidentally cleared state between calls, it would report 100 changed. */
    rc = smc_state_diff_indexed_batch(ctx, states, 100, 4, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "second batch");
    if (dirty_count != 0) {
        fprintf(stderr, "FAIL: second batch reported %zu changed, expected 0 (accidental reset?)\n", dirty_count);
        return 1;
    }
    
    smc_context_destroy(ctx);
    printf("  test_batch_no_accidental_reset: PASS\n");
    return 0;
}

/* Helper: verify a 3-frame workload for a given state size and stride.
 * Frame 1: all changed. Frame 2: all unchanged. Frame 3: one changed. */
static int run_kernel_equivalence_workload(size_t state_size, size_t stride) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    const size_t COUNT = 100;
    smc_state_indexed_config_t config = {
        .count = COUNT,
        .state_size = state_size,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    rc = smc_state_indexed_reset_stats(ctx);
    ASSERT_OK(rc, "smc_state_indexed_reset_stats");
    
    uint8_t *states = (uint8_t *)malloc(COUNT * stride);
    if (!states) {
        smc_context_destroy(ctx);
        return 1;
    }
    
    for (size_t i = 0; i < COUNT; i++) {
        uint8_t *slot = states + i * stride;
        memset(slot, 0, stride);
        if (state_size >= 1) slot[0] = (uint8_t)(i & 0xFF);
        if (state_size >= 2) slot[1] = (uint8_t)((i >> 8) & 0xFF);
        if (state_size >= 4) {
            uint32_t val = (uint32_t)i;
            memcpy(slot, &val, 4);
        }
        if (state_size >= 8) {
            uint64_t val = (uint64_t)i;
            memcpy(slot, &val, 8);
        }
        if (state_size == 16) {
            uint64_t lo = (uint64_t)i;
            uint64_t hi = (uint64_t)(i + 1);
            memcpy(slot, &lo, 8);
            memcpy(slot + 8, &hi, 8);
        }
    }
    
    uint32_t dirty_indices[COUNT];
    size_t dirty_count = 0;
    smc_state_indexed_stats_t stats;
    
    /* Frame 1: all changed */
    rc = smc_state_diff_indexed_batch(ctx, states, COUNT, stride, dirty_indices, COUNT, &dirty_count);
    ASSERT_OK(rc, "frame 1 batch");
    ASSERT_EQ(dirty_count, COUNT, "frame 1 all changed");
    for (size_t i = 0; i < COUNT; i++) {
        ASSERT_EQ(dirty_indices[i], (uint32_t)i, "frame 1 dirty index order");
    }
    rc = smc_state_indexed_get_stats(ctx, &stats);
    ASSERT_OK(rc, "frame 1 stats");
    ASSERT_EQ(stats.checks, COUNT, "frame 1 checks");
    ASSERT_EQ(stats.changed, COUNT, "frame 1 changed");
    ASSERT_EQ(stats.unchanged, 0u, "frame 1 unchanged");
    ASSERT_EQ(stats.stores, COUNT, "frame 1 stores");
    ASSERT_EQ(stats.bytes_compared, COUNT * state_size, "frame 1 bytes_compared");
    
    /* Frame 2: all unchanged */
    dirty_count = 0;
    rc = smc_state_diff_indexed_batch(ctx, states, COUNT, stride, dirty_indices, COUNT, &dirty_count);
    ASSERT_OK(rc, "frame 2 batch");
    ASSERT_EQ(dirty_count, 0u, "frame 2 no dirty");
    rc = smc_state_indexed_get_stats(ctx, &stats);
    ASSERT_OK(rc, "frame 2 stats");
    ASSERT_EQ(stats.checks, COUNT * 2, "frame 2 checks");
    ASSERT_EQ(stats.changed, COUNT, "frame 2 changed");
    ASSERT_EQ(stats.unchanged, COUNT, "frame 2 unchanged");
    ASSERT_EQ(stats.stores, COUNT, "frame 2 stores");
    
    /* Frame 3: one changed */
    size_t modified_index = 50;
    uint8_t *modified_slot = states + modified_index * stride;
    if (state_size >= 1) modified_slot[0] = 0xFF;
    if (state_size >= 2) modified_slot[1] = 0xFF;
    if (state_size >= 4) {
        uint32_t val = 0xFFFFFFFFu;
        memcpy(modified_slot, &val, 4);
    }
    if (state_size >= 8) {
        uint64_t val = 0xFFFFFFFFFFFFFFFFull;
        memcpy(modified_slot, &val, 8);
    }
    if (state_size == 16) {
        uint64_t lo = 0xFFFFFFFFFFFFFFFFull;
        uint64_t hi = 0xFFFFFFFFFFFFFFFFull;
        memcpy(modified_slot, &lo, 8);
        memcpy(modified_slot + 8, &hi, 8);
    }
    
    dirty_count = 0;
    rc = smc_state_diff_indexed_batch(ctx, states, COUNT, stride, dirty_indices, COUNT, &dirty_count);
    ASSERT_OK(rc, "frame 3 batch");
    ASSERT_EQ(dirty_count, 1u, "frame 3 one dirty");
    ASSERT_EQ(dirty_indices[0], (uint32_t)modified_index, "frame 3 dirty index");
    rc = smc_state_indexed_get_stats(ctx, &stats);
    ASSERT_OK(rc, "frame 3 stats");
    ASSERT_EQ(stats.checks, COUNT * 3, "frame 3 checks");
    ASSERT_EQ(stats.changed, COUNT + 1, "frame 3 changed");
    ASSERT_EQ(stats.unchanged, (COUNT * 2) - 1, "frame 3 unchanged");
    ASSERT_EQ(stats.stores, COUNT + 1, "frame 3 stores");
    
    free(states);
    smc_context_destroy(ctx);
    return 0;
}

static int test_kernel_equivalence_1_byte(void) {
    if (run_kernel_equivalence_workload(1, 1) != 0) return 1;
    printf("  test_kernel_equivalence_1_byte: PASS\n");
    return 0;
}

static int test_kernel_equivalence_2_bytes(void) {
    if (run_kernel_equivalence_workload(2, 2) != 0) return 1;
    printf("  test_kernel_equivalence_2_bytes: PASS\n");
    return 0;
}

static int test_kernel_equivalence_4_bytes(void) {
    if (run_kernel_equivalence_workload(4, 4) != 0) return 1;
    printf("  test_kernel_equivalence_4_bytes: PASS\n");
    return 0;
}

static int test_kernel_equivalence_8_bytes(void) {
    if (run_kernel_equivalence_workload(8, 8) != 0) return 1;
    printf("  test_kernel_equivalence_8_bytes: PASS\n");
    return 0;
}

static int test_kernel_equivalence_16_bytes(void) {
    if (run_kernel_equivalence_workload(16, 16) != 0) return 1;
    printf("  test_kernel_equivalence_16_bytes: PASS\n");
    return 0;
}

/* Test unaligned input buffers for fixed-size kernels.
 * Data is written with memcpy to avoid relying on typed pointer alignment. */
static int run_unaligned_test(size_t state_size) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    const size_t COUNT = 100;
    const size_t STRIDE = state_size * 2; /* stride > state_size, offset by 1 */
    smc_state_indexed_config_t config = {
        .count = COUNT,
        .state_size = state_size,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    /* Allocate extra byte at start so buffer+1 is valid for all records */
    uint8_t *buffer = (uint8_t *)malloc(1 + COUNT * STRIDE);
    if (!buffer) {
        smc_context_destroy(ctx);
        return 1;
    }
    
    for (size_t i = 0; i < COUNT; i++) {
        uint8_t *state_ptr = buffer + 1 + i * STRIDE;
        memset(state_ptr, 0, STRIDE);
        if (state_size == 2) {
            uint16_t val = (uint16_t)i;
            memcpy(state_ptr, &val, 2);
        } else if (state_size == 4) {
            uint32_t val = (uint32_t)i;
            memcpy(state_ptr, &val, 4);
        } else if (state_size == 8) {
            uint64_t val = (uint64_t)i;
            memcpy(state_ptr, &val, 8);
        } else if (state_size == 16) {
            uint64_t lo = (uint64_t)i;
            uint64_t hi = (uint64_t)(i + 1);
            memcpy(state_ptr, &lo, 8);
            memcpy(state_ptr + 8, &hi, 8);
        }
    }
    
    uint32_t dirty_indices[COUNT];
    size_t dirty_count = 0;
    
    rc = smc_state_diff_indexed_batch(ctx, buffer + 1, COUNT, STRIDE, dirty_indices, COUNT, &dirty_count);
    ASSERT_OK(rc, "unaligned batch");
    ASSERT_EQ(dirty_count, COUNT, "unaligned all changed");
    for (size_t i = 0; i < COUNT; i++) {
        ASSERT_EQ(dirty_indices[i], (uint32_t)i, "unaligned dirty index order");
    }
    
    /* Second identical batch should be unchanged */
    dirty_count = 0;
    rc = smc_state_diff_indexed_batch(ctx, buffer + 1, COUNT, STRIDE, dirty_indices, COUNT, &dirty_count);
    ASSERT_OK(rc, "unaligned second batch");
    ASSERT_EQ(dirty_count, 0u, "unaligned second batch unchanged");
    
    free(buffer);
    smc_context_destroy(ctx);
    return 0;
}

static int test_unaligned_2_bytes(void) {
    if (run_unaligned_test(2) != 0) return 1;
    printf("  test_unaligned_2_bytes: PASS\n");
    return 0;
}

static int test_unaligned_4_bytes(void) {
    if (run_unaligned_test(4) != 0) return 1;
    printf("  test_unaligned_4_bytes: PASS\n");
    return 0;
}

static int test_unaligned_8_bytes(void) {
    if (run_unaligned_test(8) != 0) return 1;
    printf("  test_unaligned_8_bytes: PASS\n");
    return 0;
}

static int test_unaligned_16_bytes(void) {
    if (run_unaligned_test(16) != 0) return 1;
    printf("  test_unaligned_16_bytes: PASS\n");
    return 0;
}

/* Test stride > state_size for every fixed-size kernel. */
static int run_stride_larger_test(size_t state_size) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    const size_t COUNT = 100;
    const size_t STRIDE = state_size + 4; /* extra padding */
    smc_state_indexed_config_t config = {
        .count = COUNT,
        .state_size = state_size,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    uint8_t *buffer = (uint8_t *)malloc(COUNT * STRIDE);
    if (!buffer) {
        smc_context_destroy(ctx);
        return 1;
    }
    
    for (size_t i = 0; i < COUNT; i++) {
        uint8_t *state_ptr = buffer + i * STRIDE;
        memset(state_ptr, 0xAA, STRIDE); /* fill padding with deterministic bytes */
        if (state_size == 1) {
            state_ptr[0] = (uint8_t)i;
        } else if (state_size == 2) {
            uint16_t val = (uint16_t)i;
            memcpy(state_ptr, &val, 2);
        } else if (state_size == 4) {
            uint32_t val = (uint32_t)i;
            memcpy(state_ptr, &val, 4);
        } else if (state_size == 8) {
            uint64_t val = (uint64_t)i;
            memcpy(state_ptr, &val, 8);
        } else if (state_size == 16) {
            uint64_t lo = (uint64_t)i;
            uint64_t hi = (uint64_t)(i + 1);
            memcpy(state_ptr, &lo, 8);
            memcpy(state_ptr + 8, &hi, 8);
        }
    }
    
    uint32_t dirty_indices[COUNT];
    size_t dirty_count = 0;
    
    rc = smc_state_diff_indexed_batch(ctx, buffer, COUNT, STRIDE, dirty_indices, COUNT, &dirty_count);
    ASSERT_OK(rc, "stride larger batch");
    ASSERT_EQ(dirty_count, COUNT, "stride larger all changed");
    
    /* Modify padding only; state bytes unchanged -> should report unchanged */
    for (size_t i = 0; i < COUNT; i++) {
        uint8_t *state_ptr = buffer + i * STRIDE;
        memset(state_ptr + state_size, 0xBB, STRIDE - state_size);
    }
    dirty_count = 0;
    rc = smc_state_diff_indexed_batch(ctx, buffer, COUNT, STRIDE, dirty_indices, COUNT, &dirty_count);
    ASSERT_OK(rc, "stride larger second batch");
    ASSERT_EQ(dirty_count, 0u, "stride larger padding change ignored");
    
    free(buffer);
    smc_context_destroy(ctx);
    return 0;
}

static int test_stride_larger_kernel_1_byte(void) {
    if (run_stride_larger_test(1) != 0) return 1;
    printf("  test_stride_larger_kernel_1_byte: PASS\n");
    return 0;
}

static int test_stride_larger_kernel_2_bytes(void) {
    if (run_stride_larger_test(2) != 0) return 1;
    printf("  test_stride_larger_kernel_2_bytes: PASS\n");
    return 0;
}

static int test_stride_larger_kernel_4_bytes(void) {
    if (run_stride_larger_test(4) != 0) return 1;
    printf("  test_stride_larger_kernel_4_bytes: PASS\n");
    return 0;
}

static int test_stride_larger_kernel_8_bytes(void) {
    if (run_stride_larger_test(8) != 0) return 1;
    printf("  test_stride_larger_kernel_8_bytes: PASS\n");
    return 0;
}

static int test_stride_larger_kernel_16_bytes(void) {
    if (run_stride_larger_test(16) != 0) return 1;
    printf("  test_stride_larger_kernel_16_bytes: PASS\n");
    return 0;
}

/* Test generic fallback for non-power-of-two state sizes. */
static int run_generic_fallback_test(size_t state_size) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    const size_t COUNT = 100;
    smc_state_indexed_config_t config = {
        .count = COUNT,
        .state_size = state_size,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    uint8_t *states = (uint8_t *)malloc(COUNT * state_size);
    if (!states) {
        smc_context_destroy(ctx);
        return 1;
    }
    for (size_t i = 0; i < COUNT; i++) {
        memset(states + i * state_size, (uint8_t)i, state_size);
    }
    
    uint32_t dirty_indices[COUNT];
    size_t dirty_count = 0;
    
    rc = smc_state_diff_indexed_batch(ctx, states, COUNT, state_size, dirty_indices, COUNT, &dirty_count);
    ASSERT_OK(rc, "generic fallback batch");
    ASSERT_EQ(dirty_count, COUNT, "generic fallback all changed");
    
    dirty_count = 0;
    rc = smc_state_diff_indexed_batch(ctx, states, COUNT, state_size, dirty_indices, COUNT, &dirty_count);
    ASSERT_OK(rc, "generic fallback second batch");
    ASSERT_EQ(dirty_count, 0u, "generic fallback unchanged");
    
    free(states);
    smc_context_destroy(ctx);
    return 0;
}

static int test_generic_fallback_7_bytes(void) {
    if (run_generic_fallback_test(7) != 0) return 1;
    printf("  test_generic_fallback_7_bytes: PASS\n");
    return 0;
}

static int test_generic_fallback_12_bytes(void) {
    if (run_generic_fallback_test(12) != 0) return 1;
    printf("  test_generic_fallback_12_bytes: PASS\n");
    return 0;
}

static int test_generic_fallback_24_bytes(void) {
    if (run_generic_fallback_test(24) != 0) return 1;
    printf("  test_generic_fallback_24_bytes: PASS\n");
    return 0;
}

static int test_generic_fallback_32_bytes(void) {
    if (run_generic_fallback_test(32) != 0) return 1;
    printf("  test_generic_fallback_32_bytes: PASS\n");
    return 0;
}

/* Test capacity overflow for a fixed-size kernel (8 bytes). */
static int test_fixed_kernel_capacity_overflow(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 8,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "smc_state_indexed_configure");
    
    rc = smc_state_indexed_reset_stats(ctx);
    ASSERT_OK(rc, "smc_state_indexed_reset_stats");
    
    uint8_t states[100 * 8];
    for (size_t i = 0; i < 100; i++) {
        uint64_t val = (uint64_t)i;
        memcpy(states + i * 8, &val, 8);
    }
    
    uint32_t dirty_indices[5] = {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
    size_t dirty_count = 0;
    
    rc = smc_state_diff_indexed_batch(ctx, states, 100, 8, dirty_indices, 5, &dirty_count);
    ASSERT_OK(rc, "fixed kernel capacity overflow batch");
    ASSERT_EQ(dirty_count, 100u, "fixed kernel full dirty count");
    for (size_t i = 0; i < 5; i++) {
        ASSERT_EQ(dirty_indices[i], (uint32_t)i, "fixed kernel partial indices");
    }
    
    smc_state_indexed_stats_t stats;
    rc = smc_state_indexed_get_stats(ctx, &stats);
    ASSERT_OK(rc, "fixed kernel stats");
    ASSERT_EQ(stats.checks, 100u, "fixed kernel checks");
    ASSERT_EQ(stats.changed, 100u, "fixed kernel changed");
    ASSERT_EQ(stats.stores, 100u, "fixed kernel stores");
    
    /* Second identical batch should be unchanged because state was stored */
    dirty_count = 0;
    rc = smc_state_diff_indexed_batch(ctx, states, 100, 8, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "fixed kernel second batch");
    ASSERT_EQ(dirty_count, 0u, "fixed kernel second batch unchanged");
    
    smc_context_destroy(ctx);
    printf("  test_fixed_kernel_capacity_overflow: PASS\n");
    return 0;
}

/* Test feature flag */
static int test_feature_flag(void) {
    uint32_t features = smc_features();
    if (!(features & SMC_FEATURE_INDEXED_STATE_TRACKING)) {
        fprintf(stderr, "FAIL: SMC_FEATURE_INDEXED_STATE_TRACKING not set\n");
        return 1;
    }
    printf("  test_feature_flag: PASS\n");
    return 0;
}

/* -------------------------------------------------------------------------- */
/* Stream diff tests (ABI v2.2)                                               */
/* -------------------------------------------------------------------------- */

/* Test stream diff: first observation all changed */
static int test_streams_first_observation(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 3,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "configure");
    
    uint8_t field_a[100];
    uint8_t field_b[100];
    for (int i = 0; i < 100; i++) {
        field_a[i] = (uint8_t)i;
        field_b[i] = (uint8_t)(i + 1);
    }
    
    smc_state_stream_t streams[2] = {
        {field_a, 1, 1},
        {field_b, 2, 2},
    };
    
    uint32_t dirty_indices[100];
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 2, 100, dirty_indices, 100, &dirty_count);
    ASSERT_OK(rc, "first stream diff");
    ASSERT_EQ(dirty_count, 100u, "all changed on first observation");
    
    for (size_t i = 0; i < 100; i++) {
        ASSERT_EQ(dirty_indices[i], (uint32_t)i, "dirty indices in order");
    }
    
    smc_context_destroy(ctx);
    printf("  test_streams_first_observation: PASS\n");
    return 0;
}

/* Test stream diff: second identical call all unchanged */
static int test_streams_same_all(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 3,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "configure");
    
    uint8_t field_a[100];
    uint8_t field_b[100];
    for (int i = 0; i < 100; i++) {
        field_a[i] = (uint8_t)i;
        field_b[i] = (uint8_t)(i + 1);
    }
    
    smc_state_stream_t streams[2] = {
        {field_a, 1, 1},
        {field_b, 2, 2},
    };
    
    uint32_t dirty_indices[100];
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 2, 100, dirty_indices, 100, &dirty_count);
    ASSERT_OK(rc, "first stream diff");
    ASSERT_EQ(dirty_count, 100u, "all changed on first observation");
    
    dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 2, 100, dirty_indices, 100, &dirty_count);
    ASSERT_OK(rc, "second stream diff");
    ASSERT_EQ(dirty_count, 0u, "no dirty on identical second call");
    
    smc_context_destroy(ctx);
    printf("  test_streams_same_all: PASS\n");
    return 0;
}

/* Test stream diff: one changed field marks one dirty record */
static int test_streams_one_changed_field(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 3,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "configure");
    
    uint8_t field_a[100];
    uint8_t field_b[100];
    for (int i = 0; i < 100; i++) {
        field_a[i] = (uint8_t)i;
        field_b[i] = (uint8_t)(i + 1);
    }
    
    smc_state_stream_t streams[2] = {
        {field_a, 1, 1},
        {field_b, 2, 2},
    };
    
    uint32_t dirty_indices[100];
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 2, 100, dirty_indices, 100, &dirty_count);
    ASSERT_OK(rc, "first stream diff");
    
    /* Change only field_a[42] */
    field_a[42] = 0xFF;
    
    dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 2, 100, dirty_indices, 100, &dirty_count);
    ASSERT_OK(rc, "second stream diff");
    ASSERT_EQ(dirty_count, 1u, "one dirty record");
    ASSERT_EQ(dirty_indices[0], 42u, "dirty index is 42");
    
    smc_context_destroy(ctx);
    printf("  test_streams_one_changed_field: PASS\n");
    return 0;
}

/* Test stream diff: multiple stream fields changed */
static int test_streams_multiple_fields_changed(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 7,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "configure");
    
    uint8_t field_a[100];
    uint8_t field_b[100 * 3];
    uint8_t field_c[100 * 3];
    for (int i = 0; i < 100; i++) {
        field_a[i] = 1;
        memset(&field_b[i * 3], 2, 3);
        memset(&field_c[i * 3], 3, 3);
    }
    
    smc_state_stream_t streams[3] = {
        {field_a, 1, 1},
        {field_b, 3, 3},
        {field_c, 3, 3},
    };
    
    uint32_t dirty_indices[100];
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 3, 100, dirty_indices, 100, &dirty_count);
    ASSERT_OK(rc, "first stream diff");
    ASSERT_EQ(dirty_count, 100u, "all changed on first observation");
    
    /* Change field_a and field_b in record 42 */
    field_a[42] = 99;
    memset(&field_b[42 * 3], 99, 3);
    
    dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 3, 100, dirty_indices, 100, &dirty_count);
    ASSERT_OK(rc, "second stream diff");
    ASSERT_EQ(dirty_count, 1u, "one dirty record");
    ASSERT_EQ(dirty_indices[0], 42u, "dirty index is 42");
    
    smc_context_destroy(ctx);
    printf("  test_streams_multiple_fields_changed: PASS\n");
    return 0;
}

/* Test stream diff: multiple records changed */
static int test_streams_multiple_records_changed(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 2,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "configure");
    
    uint8_t field_a[100];
    uint8_t field_b[100];
    for (int i = 0; i < 100; i++) {
        field_a[i] = (uint8_t)i;
        field_b[i] = (uint8_t)i;
    }
    
    smc_state_stream_t streams[2] = {
        {field_a, 1, 1},
        {field_b, 1, 1},
    };
    
    uint32_t dirty_indices[100];
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 2, 100, dirty_indices, 100, &dirty_count);
    ASSERT_OK(rc, "first stream diff");
    
    /* Change records 10, 20, 30 */
    field_a[10] = 0xFF;
    field_b[20] = 0xFF;
    field_a[30] = 0xFF;
    field_b[30] = 0xFF;
    
    dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 2, 100, dirty_indices, 100, &dirty_count);
    ASSERT_OK(rc, "second stream diff");
    ASSERT_EQ(dirty_count, 3u, "three dirty records");
    ASSERT_EQ(dirty_indices[0], 10u, "first dirty index");
    ASSERT_EQ(dirty_indices[1], 20u, "second dirty index");
    ASSERT_EQ(dirty_indices[2], 30u, "third dirty index");
    
    smc_context_destroy(ctx);
    printf("  test_streams_multiple_records_changed: PASS\n");
    return 0;
}

/* Test stream diff: dirty indices in ascending order */
static int test_streams_dirty_indices_ascending(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 2,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "configure");
    
    uint8_t field_a[100];
    uint8_t field_b[100];
    for (int i = 0; i < 100; i++) {
        field_a[i] = (uint8_t)i;
        field_b[i] = (uint8_t)(i ^ 0xFF);
    }
    
    smc_state_stream_t streams[2] = {
        {field_a, 1, 1},
        {field_b, 1, 1},
    };
    
    uint32_t dirty_indices[100];
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 2, 100, dirty_indices, 100, &dirty_count);
    ASSERT_OK(rc, "first stream diff");
    
    /* Change every 10th record */
    for (int i = 0; i < 100; i += 10) {
        field_a[i] = (uint8_t)(field_a[i] + 1);
    }
    
    dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 2, 100, dirty_indices, 100, &dirty_count);
    ASSERT_OK(rc, "second stream diff");
    ASSERT_EQ(dirty_count, 10u, "ten dirty records");
    
    for (size_t i = 0; i < 10; i++) {
        ASSERT_EQ(dirty_indices[i], (uint32_t)(i * 10), "ascending dirty indices");
    }
    
    smc_context_destroy(ctx);
    printf("  test_streams_dirty_indices_ascending: PASS\n");
    return 0;
}

/* Test stream diff: capacity overflow updates all stored state */
static int test_streams_capacity_overflow(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 2,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "configure");
    
    uint8_t field_a[100];
    uint8_t field_b[100];
    for (int i = 0; i < 100; i++) {
        field_a[i] = (uint8_t)i;
        field_b[i] = (uint8_t)i;
    }
    
    smc_state_stream_t streams[2] = {
        {field_a, 1, 1},
        {field_b, 1, 1},
    };
    
    uint32_t dirty_indices[5] = {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 2, 100, dirty_indices, 5, &dirty_count);
    ASSERT_OK(rc, "stream diff with capacity overflow");
    ASSERT_EQ(dirty_count, 100u, "full dirty count reported");
    
    for (size_t i = 0; i < 5; i++) {
        ASSERT_EQ(dirty_indices[i], (uint32_t)i, "partial indices written");
    }
    
    /* Second identical call should be unchanged because state was stored */
    dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 2, 100, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "second stream diff");
    ASSERT_EQ(dirty_count, 0u, "second call unchanged");
    
    smc_context_destroy(ctx);
    printf("  test_streams_capacity_overflow: PASS\n");
    return 0;
}

/* Test stream diff: NULL streams validation */
static int test_streams_null_streams(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 2,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "configure");
    
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, NULL, 2, 100, NULL, 0, &dirty_count);
    if (rc != SMC_ERR_INVALID) {
        fprintf(stderr, "FAIL: NULL streams with record_count>0 should return SMC_ERR_INVALID, got %d\n", rc);
        return 1;
    }
    
    smc_context_destroy(ctx);
    printf("  test_streams_null_streams: PASS\n");
    return 0;
}

/* Test stream diff: NULL stream data with zero-size field allowed */
static int test_streams_null_data_zero_size(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 1,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "configure");
    
    uint8_t field_a[100];
    for (int i = 0; i < 100; i++) {
        field_a[i] = (uint8_t)i;
    }
    
    smc_state_stream_t streams[2] = {
        {field_a, 1, 1},
        {NULL, 1, 0},  /* zero-size stream, data may be NULL */
    };
    
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 2, 100, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "stream diff with zero-size stream");
    ASSERT_EQ(dirty_count, 100u, "all changed on first observation");
    
    smc_context_destroy(ctx);
    printf("  test_streams_null_data_zero_size: PASS\n");
    return 0;
}

/* Test stream diff: zero-size stream behavior */
static int test_streams_zero_total_size(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 0,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "configure");
    
    smc_state_stream_t streams[2] = {
        {NULL, 1, 0},
        {NULL, 1, 0},
    };
    
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 2, 100, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "first zero-size stream diff");
    ASSERT_EQ(dirty_count, 100u, "all changed on first observation");
    
    dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 2, 100, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "second zero-size stream diff");
    ASSERT_EQ(dirty_count, 0u, "all unchanged on second observation");
    
    smc_context_destroy(ctx);
    printf("  test_streams_zero_total_size: PASS\n");
    return 0;
}

/* Test stream diff: stride larger than field_size */
static int test_streams_stride_larger(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 2,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "configure");
    
    /* Strided arrays: field_a at offset 0, field_b at offset 2, stride 4 */
    uint8_t buffer[100 * 4];
    for (int i = 0; i < 100; i++) {
        buffer[i * 4 + 0] = (uint8_t)i;
        buffer[i * 4 + 2] = (uint8_t)(i + 1);
    }
    
    smc_state_stream_t streams[2] = {
        {buffer + 0, 4, 1},
        {buffer + 2, 4, 1},
    };
    
    uint32_t dirty_indices[100];
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 2, 100, dirty_indices, 100, &dirty_count);
    ASSERT_OK(rc, "stream diff with larger stride");
    ASSERT_EQ(dirty_count, 100u, "all changed with larger stride");
    
    smc_context_destroy(ctx);
    printf("  test_streams_stride_larger: PASS\n");
    return 0;
}

/* Test stream diff: stride smaller than field_size rejected */
static int test_streams_stride_too_small(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 2,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "configure");
    
    uint8_t field_a[100];
    smc_state_stream_t streams[1] = {
        {field_a, 1, 2},  /* stride 1 < field_size 2 */
    };
    
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 1, 100, NULL, 0, &dirty_count);
    if (rc != SMC_ERR_SIZE) {
        fprintf(stderr, "FAIL: stride < field_size should return SMC_ERR_SIZE, got %d\n", rc);
        return 1;
    }
    
    smc_context_destroy(ctx);
    printf("  test_streams_stride_too_small: PASS\n");
    return 0;
}

/* Test stream diff: record_count too large rejected */
static int test_streams_record_count_too_large(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 1,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "configure");
    
    uint8_t field_a[200];
    smc_state_stream_t streams[1] = {
        {field_a, 1, 1},
    };
    
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 1, 200, NULL, 0, &dirty_count);
    if (rc != SMC_ERR_SIZE) {
        fprintf(stderr, "FAIL: record_count > configured count should return SMC_ERR_SIZE, got %d\n", rc);
        return 1;
    }
    
    smc_context_destroy(ctx);
    printf("  test_streams_record_count_too_large: PASS\n");
    return 0;
}

/* Test stream diff: total field size mismatch rejected */
static int test_streams_total_size_mismatch(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 8,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "configure");
    
    uint8_t field_a[100];
    smc_state_stream_t streams[1] = {
        {field_a, 1, 7},  /* total field size 7 != configured 8 */
    };
    
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 1, 100, NULL, 0, &dirty_count);
    if (rc != SMC_ERR_SIZE) {
        fprintf(stderr, "FAIL: total field size mismatch should return SMC_ERR_SIZE, got %d\n", rc);
        return 1;
    }
    
    smc_context_destroy(ctx);
    printf("  test_streams_total_size_mismatch: PASS\n");
    return 0;
}

/* Test stream diff: mixed-size streams 1+3+3 */
static int test_streams_mixed_1_3_3(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 7,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "configure");
    
    uint8_t field_a[100];
    uint8_t field_b[100 * 3];
    uint8_t field_c[100 * 3];
    for (int i = 0; i < 100; i++) {
        field_a[i] = 1;
        memset(&field_b[i * 3], 2, 3);
        memset(&field_c[i * 3], 3, 3);
    }
    
    smc_state_stream_t streams[3] = {
        {field_a, 1, 1},
        {field_b, 3, 3},
        {field_c, 3, 3},
    };
    
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 3, 100, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "mixed 1+3+3 first diff");
    ASSERT_EQ(dirty_count, 100u, "all changed");
    
    /* Change only field_b in record 50 */
    memset(&field_b[50 * 3], 99, 3);
    dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 3, 100, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "mixed 1+3+3 second diff");
    ASSERT_EQ(dirty_count, 1u, "one dirty");
    
    smc_context_destroy(ctx);
    printf("  test_streams_mixed_1_3_3: PASS\n");
    return 0;
}

/* Test stream diff: mixed-size streams 1+2+4 */
static int test_streams_mixed_1_2_4(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 7,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "configure");
    
    uint8_t field_a[100];
    uint8_t field_b[100 * 2];
    uint8_t field_c[100 * 4];
    for (int i = 0; i < 100; i++) {
        field_a[i] = 1;
        memset(&field_b[i * 2], 2, 2);
        memset(&field_c[i * 4], 3, 4);
    }
    
    smc_state_stream_t streams[3] = {
        {field_a, 1, 1},
        {field_b, 2, 2},
        {field_c, 4, 4},
    };
    
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 3, 100, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "mixed 1+2+4 first diff");
    ASSERT_EQ(dirty_count, 100u, "all changed");
    
    /* Change only field_c in record 50 */
    memset(&field_c[50 * 4], 99, 4);
    dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 3, 100, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "mixed 1+2+4 second diff");
    ASSERT_EQ(dirty_count, 1u, "one dirty");
    
    smc_context_destroy(ctx);
    printf("  test_streams_mixed_1_2_4: PASS\n");
    return 0;
}

/* Test stream diff: seven 1-byte streams */
static int test_streams_seven_one_byte(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 7,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "configure");
    
    uint8_t fields[7][100];
    for (int s = 0; s < 7; s++) {
        for (int i = 0; i < 100; i++) {
            fields[s][i] = (uint8_t)(s + 1);
        }
    }
    
    smc_state_stream_t streams[7];
    for (int s = 0; s < 7; s++) {
        streams[s].data = fields[s];
        streams[s].stride = 1;
        streams[s].field_size = 1;
    }
    
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 7, 100, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "seven 1-byte first diff");
    ASSERT_EQ(dirty_count, 100u, "all changed");
    
    /* Change field 3 in record 50 */
    fields[3][50] = 0xFF;
    dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 7, 100, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "seven 1-byte second diff");
    ASSERT_EQ(dirty_count, 1u, "one dirty");
    
    smc_context_destroy(ctx);
    printf("  test_streams_seven_one_byte: PASS\n");
    return 0;
}

/* Test stream diff: AoS layout with mixed fields */
static int test_streams_aos_mixed(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    typedef struct {
        uint8_t glyph;
        uint8_t fg[3];
        uint8_t bg[3];
    } Cell;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = sizeof(Cell),
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "configure");
    
    Cell cells[100];
    for (int i = 0; i < 100; i++) {
        cells[i].glyph = (uint8_t)i;
        memset(cells[i].fg, (uint8_t)(i + 1), 3);
        memset(cells[i].bg, (uint8_t)(i + 2), 3);
    }
    
    smc_state_stream_t streams[3] = {
        {&cells[0].glyph, sizeof(Cell), 1},
        {&cells[0].fg[0], sizeof(Cell), 3},
        {&cells[0].bg[0], sizeof(Cell), 3},
    };
    
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 3, 100, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "AoS mixed first diff");
    ASSERT_EQ(dirty_count, 100u, "all changed");
    
    cells[50].glyph = 0xFF;
    dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 3, 100, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "AoS mixed second diff");
    ASSERT_EQ(dirty_count, 1u, "one dirty");
    
    smc_context_destroy(ctx);
    printf("  test_streams_aos_mixed: PASS\n");
    return 0;
}

/* Test stream diff: SoA layout with mixed fields */
static int test_streams_soa_mixed(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 7,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "configure");
    
    uint8_t glyphs[100];
    uint8_t fg[100 * 3];
    uint8_t bg[100 * 3];
    for (int i = 0; i < 100; i++) {
        glyphs[i] = (uint8_t)i;
        memset(&fg[i * 3], (uint8_t)(i + 1), 3);
        memset(&bg[i * 3], (uint8_t)(i + 2), 3);
    }
    
    smc_state_stream_t streams[3] = {
        {glyphs, 1, 1},
        {fg, 3, 3},
        {bg, 3, 3},
    };
    
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 3, 100, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "SoA mixed first diff");
    ASSERT_EQ(dirty_count, 100u, "all changed");
    
    glyphs[50] = 0xFF;
    dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 3, 100, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "SoA mixed second diff");
    ASSERT_EQ(dirty_count, 1u, "one dirty");
    
    smc_context_destroy(ctx);
    printf("  test_streams_soa_mixed: PASS\n");
    return 0;
}

/* Test stream diff: overlapping streams behavior */
static int test_streams_overlapping(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 2,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "configure");
    
    uint8_t data[100];
    for (int i = 0; i < 100; i++) {
        data[i] = (uint8_t)i;
    }
    
    /* Two streams reading from the same byte */
    smc_state_stream_t streams[2] = {
        {data, 1, 1},
        {data, 1, 1},
    };
    
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 2, 100, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "overlapping first diff");
    ASSERT_EQ(dirty_count, 100u, "all changed");
    
    /* No changes */
    dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 2, 100, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "overlapping second diff");
    ASSERT_EQ(dirty_count, 0u, "unchanged");
    
    smc_context_destroy(ctx);
    printf("  test_streams_overlapping: PASS\n");
    return 0;
}

/* Test stream diff: stats correctness */
static int test_streams_stats(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 2,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "configure");
    
    rc = smc_state_indexed_reset_stats(ctx);
    ASSERT_OK(rc, "reset stats");
    
    uint8_t field_a[100];
    uint8_t field_b[100];
    for (int i = 0; i < 100; i++) {
        field_a[i] = (uint8_t)i;
        field_b[i] = (uint8_t)i;
    }
    
    smc_state_stream_t streams[2] = {
        {field_a, 1, 1},
        {field_b, 1, 1},
    };
    
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 2, 100, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "first diff");
    
    /* Change 10 records */
    for (int i = 0; i < 100; i += 10) {
        field_a[i] = (uint8_t)(field_a[i] + 1);
    }
    
    dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 2, 100, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "second diff");
    
    smc_state_indexed_stats_t stats;
    rc = smc_state_indexed_get_stats(ctx, &stats);
    ASSERT_OK(rc, "get stats");
    ASSERT_EQ(stats.checks, 200u, "checks");
    ASSERT_EQ(stats.changed, 110u, "changed (100 + 10)");
    ASSERT_EQ(stats.unchanged, 90u, "unchanged");
    ASSERT_EQ(stats.stores, 110u, "stores");
    ASSERT_EQ(stats.bytes_compared, 400u, "bytes_compared");
    
    smc_context_destroy(ctx);
    printf("  test_streams_stats: PASS\n");
    return 0;
}

/* Test stream diff: clear/reset behavior */
static int test_streams_clear(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 2,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "configure");
    
    uint8_t field_a[100];
    uint8_t field_b[100];
    for (int i = 0; i < 100; i++) {
        field_a[i] = (uint8_t)i;
        field_b[i] = (uint8_t)i;
    }
    
    smc_state_stream_t streams[2] = {
        {field_a, 1, 1},
        {field_b, 1, 1},
    };
    
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 2, 100, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "first diff");
    ASSERT_EQ(dirty_count, 100u, "all changed");
    
    rc = smc_state_indexed_clear(ctx);
    ASSERT_OK(rc, "clear");
    
    dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 2, 100, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "diff after clear");
    ASSERT_EQ(dirty_count, 100u, "all changed after clear");
    
    smc_context_destroy(ctx);
    printf("  test_streams_clear: PASS\n");
    return 0;
}

/* Test stream diff: stale-field prevention (critical correctness) */
static int test_streams_no_stale_field_bug(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 7,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "configure");
    
    uint8_t field_a[100];
    uint8_t field_b[100 * 3];
    uint8_t field_c[100 * 3];
    for (int i = 0; i < 100; i++) {
        field_a[i] = 1;
        memset(&field_b[i * 3], 2, 3);
        memset(&field_c[i * 3], 3, 3);
    }
    
    smc_state_stream_t streams[3] = {
        {field_a, 1, 1},
        {field_b, 3, 3},
        {field_c, 3, 3},
    };
    
    uint32_t dirty_indices[100];
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 3, 100, dirty_indices, 100, &dirty_count);
    ASSERT_OK(rc, "frame 1");
    ASSERT_EQ(dirty_count, 100u, "frame 1 all changed");
    
    /* Frame 2: change field_a and field_b in record 42 */
    field_a[42] = 99;
    memset(&field_b[42 * 3], 99, 3);
    
    dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 3, 100, dirty_indices, 100, &dirty_count);
    ASSERT_OK(rc, "frame 2");
    ASSERT_EQ(dirty_count, 1u, "frame 2 one dirty");
    ASSERT_EQ(dirty_indices[0], 42u, "frame 2 dirty index 42");
    
    /* Frame 3: same as frame 2 - should be unchanged */
    dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 3, 100, dirty_indices, 100, &dirty_count);
    ASSERT_OK(rc, "frame 3");
    ASSERT_EQ(dirty_count, 0u, "frame 3 unchanged");
    
    smc_context_destroy(ctx);
    printf("  test_streams_no_stale_field_bug: PASS\n");
    return 0;
}

/* Test stream diff: empty batch */
static int test_streams_empty_batch(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 2,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "configure");
    
    size_t dirty_count = 12345;
    rc = smc_state_diff_indexed_streams(ctx, NULL, 0, 0, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "empty batch");
    ASSERT_EQ(dirty_count, 0u, "dirty_count is 0 for empty batch");
    
    smc_context_destroy(ctx);
    printf("  test_streams_empty_batch: PASS\n");
    return 0;
}

/* Test stream diff: short-circuit stale-field prevention.
 * Change a later field first, then verify the next frame is unchanged.
 * This guards against implementations that copy only the changed field. */
static int test_streams_short_circuit_stale_field(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 7,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "configure");
    
    uint8_t field_a[100];
    uint8_t field_b[100 * 3];
    uint8_t field_c[100 * 3];
    for (int i = 0; i < 100; i++) {
        field_a[i] = 0x01;
        memset(&field_b[i * 3], 0x02, 3);
        memset(&field_c[i * 3], 0x03, 3);
    }
    
    smc_state_stream_t streams[3] = {
        {field_a, 1, 1},
        {field_b, 3, 3},
        {field_c, 3, 3},
    };
    
    uint32_t dirty_indices[100];
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 3, 100, dirty_indices, 100, &dirty_count);
    ASSERT_OK(rc, "frame 1");
    ASSERT_EQ(dirty_count, 100u, "frame 1 all changed");
    
    /* Frame 2: change only field_c in record 42 (last field) */
    field_c[42 * 3 + 0] = 0xAA;
    field_c[42 * 3 + 1] = 0xBB;
    field_c[42 * 3 + 2] = 0xCC;
    
    dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 3, 100, dirty_indices, 100, &dirty_count);
    ASSERT_OK(rc, "frame 2");
    ASSERT_EQ(dirty_count, 1u, "frame 2 one dirty");
    ASSERT_EQ(dirty_indices[0], 42u, "frame 2 dirty index 42");
    
    /* Frame 3: identical to frame 2 - should be unchanged */
    dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 3, 100, dirty_indices, 100, &dirty_count);
    ASSERT_OK(rc, "frame 3");
    ASSERT_EQ(dirty_count, 0u, "frame 3 unchanged");
    
    smc_context_destroy(ctx);
    printf("  test_streams_short_circuit_stale_field: PASS\n");
    return 0;
}

/* Test stream diff: stream order defines logical byte layout.
 * Changing stream order across calls is caller misuse and may mark records
 * dirty because the stored snapshot is reinterpreted under the new layout. */
static int test_streams_order_change_marks_dirty(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 7,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "configure");
    
    /* Asymmetric values so reordering changes the packed byte sequence. */
    uint8_t field_a[100];
    uint8_t field_b[100 * 3];
    uint8_t field_c[100 * 3];
    for (int i = 0; i < 100; i++) {
        field_a[i] = 0x01;
        field_b[i * 3 + 0] = 0x02;
        field_b[i * 3 + 1] = 0x03;
        field_b[i * 3 + 2] = 0x04;
        field_c[i * 3 + 0] = 0x05;
        field_c[i * 3 + 1] = 0x06;
        field_c[i * 3 + 2] = 0x07;
    }
    
    /* First call with [1,3,3] order */
    smc_state_stream_t streams_first[3] = {
        {field_a, 1, 1},
        {field_b, 3, 3},
        {field_c, 3, 3},
    };
    
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams_first, 3, 100, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "first call [1,3,3]");
    ASSERT_EQ(dirty_count, 100u, "first call all changed");
    
    /* Second call with [3,1,3] order: stored snapshot bytes are reinterpreted */
    smc_state_stream_t streams_second[3] = {
        {field_b, 3, 3},
        {field_a, 1, 1},
        {field_c, 3, 3},
    };
    
    dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams_second, 3, 100, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "second call [3,1,3]");
    /* With asymmetric values, the reinterpreted layout does not match. */
    ASSERT_EQ(dirty_count, 100u, "order change marks all records dirty");
    
    smc_context_destroy(ctx);
    printf("  test_streams_order_change_marks_dirty: PASS\n");
    return 0;
}

/* Test stream diff: heap fallback for stream_count > 8 */
static int test_streams_heap_fallback(void) {
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) return 1;
    
    smc_state_indexed_config_t config = {
        .count = 100,
        .state_size = 16,
        .memory_budget_bytes = 0
    };
    int rc = smc_state_indexed_configure(ctx, &config);
    ASSERT_OK(rc, "configure");
    
    uint8_t fields[16][100];
    smc_state_stream_t streams[16];
    for (int s = 0; s < 16; s++) {
        for (int i = 0; i < 100; i++) {
            fields[s][i] = (uint8_t)(s + 1);
        }
        streams[s].data = fields[s];
        streams[s].stride = 1;
        streams[s].field_size = 1;
    }
    
    size_t dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 16, 100, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "stream diff with 16 streams");
    ASSERT_EQ(dirty_count, 100u, "all changed with heap fallback");
    
    dirty_count = 0;
    rc = smc_state_diff_indexed_streams(ctx, streams, 16, 100, NULL, 0, &dirty_count);
    ASSERT_OK(rc, "second stream diff with 16 streams");
    ASSERT_EQ(dirty_count, 0u, "unchanged with heap fallback");
    
    smc_context_destroy(ctx);
    printf("  test_streams_heap_fallback: PASS\n");
    return 0;
}

int main(void) {
    if (smc_init() != SMC_OK) {
        fprintf(stderr, "FAIL: smc_init failed\n");
        return 1;
    }
    
    if (test_feature_flag() != 0) return 1;
    
    if (test_indexed_first_observation() != 0) return 1;
    if (test_indexed_same_state() != 0) return 1;
    if (test_indexed_different_state() != 0) return 1;
    if (test_indexed_zero_size_state() != 0) return 1;
    if (test_indexed_out_of_range() != 0) return 1;
    if (test_indexed_wrong_size() != 0) return 1;
    if (test_indexed_clear() != 0) return 1;
    if (test_indexed_stats() != 0) return 1;
    if (test_indexed_memory_budget() != 0) return 1;
    if (test_indexed_double_configure() != 0) return 1;
    
    if (test_batch_all_new() != 0) return 1;
    if (test_batch_same_batch() != 0) return 1;
    if (test_batch_one_changed() != 0) return 1;
    if (test_batch_null_dirty_no_write() != 0) return 1;
    if (test_batch_null_dirty_nonzero_capacity() != 0) return 1;
    if (test_batch_count_too_large() != 0) return 1;
    if (test_batch_stride_larger() != 0) return 1;
    if (test_batch_count_zero() != 0) return 1;
    if (test_batch_zero_stride() != 0) return 1;
    if (test_batch_states_null_with_size() != 0) return 1;
    if (test_batch_states_null_zero_size() != 0) return 1;
    if (test_batch_stride_too_small() != 0) return 1;
    if (test_batch_dirty_capacity_overflow() != 0) return 1;
    if (test_batch_stats_accumulation() != 0) return 1;
    if (test_batch_multiple_modified_ordered() != 0) return 1;
    if (test_renderer_shaped_workload() != 0) return 1;
    if (test_batch_no_accidental_reset() != 0) return 1;
    
    if (test_kernel_equivalence_1_byte() != 0) return 1;
    if (test_kernel_equivalence_2_bytes() != 0) return 1;
    if (test_kernel_equivalence_4_bytes() != 0) return 1;
    if (test_kernel_equivalence_8_bytes() != 0) return 1;
    if (test_kernel_equivalence_16_bytes() != 0) return 1;
    
    if (test_unaligned_2_bytes() != 0) return 1;
    if (test_unaligned_4_bytes() != 0) return 1;
    if (test_unaligned_8_bytes() != 0) return 1;
    if (test_unaligned_16_bytes() != 0) return 1;
    
    if (test_stride_larger_kernel_1_byte() != 0) return 1;
    if (test_stride_larger_kernel_2_bytes() != 0) return 1;
    if (test_stride_larger_kernel_4_bytes() != 0) return 1;
    if (test_stride_larger_kernel_8_bytes() != 0) return 1;
    if (test_stride_larger_kernel_16_bytes() != 0) return 1;
    
    if (test_generic_fallback_7_bytes() != 0) return 1;
    if (test_generic_fallback_12_bytes() != 0) return 1;
    if (test_generic_fallback_24_bytes() != 0) return 1;
    if (test_generic_fallback_32_bytes() != 0) return 1;
    
    if (test_fixed_kernel_capacity_overflow() != 0) return 1;
    
    /* Stream diff tests */
    if (test_streams_first_observation() != 0) return 1;
    if (test_streams_same_all() != 0) return 1;
    if (test_streams_one_changed_field() != 0) return 1;
    if (test_streams_multiple_fields_changed() != 0) return 1;
    if (test_streams_multiple_records_changed() != 0) return 1;
    if (test_streams_dirty_indices_ascending() != 0) return 1;
    if (test_streams_capacity_overflow() != 0) return 1;
    if (test_streams_null_streams() != 0) return 1;
    if (test_streams_null_data_zero_size() != 0) return 1;
    if (test_streams_zero_total_size() != 0) return 1;
    if (test_streams_stride_larger() != 0) return 1;
    if (test_streams_stride_too_small() != 0) return 1;
    if (test_streams_record_count_too_large() != 0) return 1;
    if (test_streams_total_size_mismatch() != 0) return 1;
    if (test_streams_mixed_1_3_3() != 0) return 1;
    if (test_streams_mixed_1_2_4() != 0) return 1;
    if (test_streams_seven_one_byte() != 0) return 1;
    if (test_streams_aos_mixed() != 0) return 1;
    if (test_streams_soa_mixed() != 0) return 1;
    if (test_streams_overlapping() != 0) return 1;
    if (test_streams_stats() != 0) return 1;
    if (test_streams_clear() != 0) return 1;
    if (test_streams_no_stale_field_bug() != 0) return 1;
    if (test_streams_empty_batch() != 0) return 1;
    if (test_streams_short_circuit_stale_field() != 0) return 1;
    if (test_streams_order_change_marks_dirty() != 0) return 1;
    if (test_streams_heap_fallback() != 0) return 1;
    
    smc_shutdown();
    printf("All indexed state tests passed.\n");
    return 0;
}
