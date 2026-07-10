/* tests/benchmarks/benchmark_indexed_state.c — microbenchmark for SMC v2.1 indexed state API
 *
 * Measures operation costs for:
 *   - generic smc_state_changed() unchanged path
 *   - indexed smc_state_changed_index() unchanged path
 *   - batch indexed unchanged path
 *   - generic changed path
 *   - indexed changed path
 *   - batch changed path
 *
 * Build: cmake -B build && cmake --build build --target benchmark_indexed_state
 * Run with:   ./build/benchmark_indexed_state
 */

#define _POSIX_C_SOURCE 200809L
#include "smc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Renderer-like workload: 260x160 = 41600 cells, but use 41600 directly */
#define NUM_RECORDS 41600
#define STATE_SIZE 8
#define NUM_ITERATIONS 10000

/* Get monotonic time in nanoseconds */
static uint64_t get_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

int main(void) {
    int rc = smc_init();
    if (rc != SMC_OK) {
        fprintf(stderr, "FAIL: smc_init failed\n");
        return 1;
    }
    
    /* Check features */
    uint32_t features = smc_features();
    if (!(features & SMC_FEATURE_STATE_TRACKING)) {
        fprintf(stderr, "FAIL: SMC_FEATURE_STATE_TRACKING not set\n");
        return 1;
    }
    if (!(features & SMC_FEATURE_INDEXED_STATE_TRACKING)) {
        fprintf(stderr, "FAIL: SMC_FEATURE_INDEXED_STATE_TRACKING not set\n");
        return 1;
    }
    
    /* Create context */
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) {
        fprintf(stderr, "FAIL: context create\n");
        return 1;
    }
    
    /* Configure generic state tracker */
    smc_state_config_t generic_config = {
        .max_entries = NUM_RECORDS * 2,
        .max_key_size = 8,
        .max_state_size = STATE_SIZE,
        .memory_budget_bytes = 0
    };
    rc = smc_state_configure(ctx, &generic_config);
    if (rc != SMC_OK) {
        fprintf(stderr, "FAIL: generic state configure\n");
        return 1;
    }
    
    /* Configure indexed state tracker */
    smc_state_indexed_config_t indexed_config = {
        .count = NUM_RECORDS,
        .state_size = STATE_SIZE,
        .memory_budget_bytes = 0
    };
    rc = smc_state_indexed_configure(ctx, &indexed_config);
    if (rc != SMC_OK) {
        fprintf(stderr, "FAIL: indexed state configure\n");
        return 1;
    }
    
    /* Create test data: 41600 records of 8 bytes each */
    uint32_t *states = (uint32_t *)malloc(NUM_RECORDS * STATE_SIZE);
    if (!states) {
        fprintf(stderr, "FAIL: out of memory for states\n");
        return 1;
    }
    
    /* Initialize states with deterministic values */
    for (size_t i = 0; i < NUM_RECORDS; i++) {
        states[i] = (uint32_t)i;
    }
    
    printf("SMC indexed state benchmark\n");
    printf("records: %d\n", NUM_RECORDS);
    printf("state size: %d\n", STATE_SIZE);
    printf("\n");
    
    uint64_t sum = 0; /* volatile sink to prevent optimization */
    
    /* Warm up generic state - populate it */
    int changed;
    for (int i = 0; i < 100; i++) {
        smc_state_changed(ctx, &i, sizeof(i), &states[i], STATE_SIZE, &changed);
    }
    
    /* Warm up indexed state */
    smc_state_indexed_reset_stats(ctx);
    smc_state_reset_stats(ctx);
    
    /* Warm up: populate indexed state with first few records */
    for (int i = 0; i < 100; i++) {
        smc_state_changed_index(ctx, i, &states[i], STATE_SIZE, &changed);
    }
    
    /* Benchmark: generic state unchanged path */
    uint64_t start = get_ns();
    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        changed = 1;
        uint32_t key = (uint32_t)(iter % 100);
        smc_state_changed(ctx, &key, sizeof(key), &states[key], STATE_SIZE, &changed);
        sum += changed;
    }
    uint64_t elapsed = get_ns() - start;
    double generic_unchanged_ns = (double)elapsed / NUM_ITERATIONS;
    printf("generic unchanged:      %.1f ns/op\n", generic_unchanged_ns);
    
    /* Benchmark: indexed state unchanged path */
    start = get_ns();
    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        changed = 1;
        uint32_t idx = (uint32_t)(iter % 100);
        smc_state_changed_index(ctx, idx, &states[idx], STATE_SIZE, &changed);
        sum += changed;
    }
    elapsed = get_ns() - start;
    double indexed_unchanged_ns = (double)elapsed / NUM_ITERATIONS;
    printf("indexed unchanged:      %.1f ns/op\n", indexed_unchanged_ns);
    
    /* Benchmark: batch indexed unchanged path */
    start = get_ns();
    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        size_t dirty_count = 0;
        smc_state_diff_indexed_batch(ctx, states, 100, STATE_SIZE, NULL, 0, &dirty_count);
        sum += dirty_count;
    }
    elapsed = get_ns() - start;
    /* Batch handles 100 records per call, so divide by 100 for per-record */
    double batch_unchanged_ns = (double)elapsed / (NUM_ITERATIONS * 100);
    printf("batch unchanged:        %.1f ns/op\n", batch_unchanged_ns);
    
    /* Reset indexed state for changed-state benchmarks */
    smc_state_indexed_clear(ctx);
    
    /* Benchmark: generic state changed path (first observation = changed) */
    start = get_ns();
    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        changed = 0;
        /* Each call is a new key so each is changed */
        uint32_t key = (uint32_t)(iter + 1000); /* Different keys */
        smc_state_changed(ctx, &key, sizeof(key), &states[key % NUM_RECORDS], STATE_SIZE, &changed);
        sum += changed;
    }
    elapsed = get_ns() - start;
    double generic_changed_ns = (double)elapsed / NUM_ITERATIONS;
    printf("generic changed:        %.1f ns/op\n", generic_changed_ns);
    
    /* Benchmark: indexed state changed path (first observation = changed) */
    start = get_ns();
    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        changed = 0;
        smc_state_changed_index(ctx, (uint32_t)iter, &states[iter % NUM_RECORDS], STATE_SIZE, &changed);
        sum += changed;
    }
    elapsed = get_ns() - start;
    double indexed_changed_ns = (double)elapsed / NUM_ITERATIONS;
    printf("indexed changed:        %.1f ns/op\n", indexed_changed_ns);
    
    /* Benchmark: batch changed path */
    uint32_t *changed_states = (uint32_t *)malloc(NUM_RECORDS * STATE_SIZE);
    if (!changed_states) {
        fprintf(stderr, "FAIL: out of memory for changed_states\n");
        return 1;
    }
    for (size_t i = 0; i < NUM_RECORDS; i++) {
        changed_states[i] = (uint32_t)(i + 100000);
    }
    
    start = get_ns();
    for (int iter = 0; iter < 1; iter++) { /* One full pass */
        size_t dirty_count = 0;
        smc_state_diff_indexed_batch(ctx, changed_states, NUM_RECORDS, STATE_SIZE, NULL, 0, &dirty_count);
        sum += dirty_count;
    }
    elapsed = get_ns() - start;
    double batch_changed_ns = (double)elapsed / NUM_RECORDS;
    printf("batch changed:          %.1f ns/op\n", batch_changed_ns);
    
    free(states);
    free(changed_states);
    
    smc_context_destroy(ctx);
    smc_shutdown();
    
    printf("\nSMC indexed state benchmark complete.\n");
    return 0;
}