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
    
    smc_shutdown();
    printf("All indexed state tests passed.\n");
    return 0;
}