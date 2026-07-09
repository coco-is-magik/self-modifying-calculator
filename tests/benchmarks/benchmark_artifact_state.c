/* tests/benchmarks/benchmark_artifact_state.c — microbenchmark for SMC v2 hot-path operations
 *
 * Measures operation costs for:
 *   - smc_state_changed() unchanged path
 *   - smc_state_changed() changed (same-size) path  
 *   - smc_artifact_lookup() hit path
 *   - smc_artifact_lookup() miss path
 *   - smc_artifact_store() same-key update path
 *
 * Build with: cmake -B build && cmake --build build --target benchmark_artifact_state
 * Run with:   ./build/benchmark_artifact_state
 */

#define _POSIX_C_SOURCE 200809L
#include "smc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NUM_ITERATIONS 1000000

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
    
    /* Configure artifact cache */
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) {
        fprintf(stderr, "FAIL: context create\n");
        return 1;
    }
    
    smc_artifact_config_t a_config = {
        .max_entries = 4096,
        .max_key_size = 64,
        .max_value_size = 256,
        .memory_budget_bytes = 0
    };
    rc = smc_artifact_configure(ctx, &a_config);
    if (rc != SMC_OK) {
        fprintf(stderr, "FAIL: artifact configure\n");
        return 1;
    }
    
    /* Configure state tracking */
    smc_state_config_t s_config = {0};
    rc = smc_state_configure(ctx, &s_config);
    if (rc != SMC_OK) {
        fprintf(stderr, "FAIL: state configure\n");
        return 1;
    }
    
    /* Pre-populate some artifacts for hit testing */
    uint32_t hit_key = 0x12345678;
    uint32_t hit_value = 0xDEADBEEF;
    uint8_t blob[256];
    memset(blob, 0xAB, sizeof(blob));
    
    for (int i = 0; i < 100; i++) {
        uint32_t k = hit_key + i;
        smc_artifact_store(ctx, &k, sizeof(k), blob, sizeof(blob));
    }
    
    /* Warm up caches */
    uint32_t key = 1;
    uint32_t state = 100;
    int changed = 0;
    for (int i = 0; i < 100; i++) {
        smc_state_changed(ctx, &key, sizeof(key), &state, sizeof(state), &changed);
    }
    
    /* Benchmark: state unchanged */
    uint64_t sum = 0; /* volatile sink to prevent optimization */
    uint64_t start = get_ns();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        changed = 0;
        smc_state_changed(ctx, &key, sizeof(key), &state, sizeof(state), &changed);
        sum += changed;
    }
    uint64_t elapsed = get_ns() - start;
    double ns_per_op = (double)elapsed / NUM_ITERATIONS;
    printf("state unchanged:      %.1f ns/op\n", ns_per_op);
    
    /* Benchmark: state changed (same-size) */
    uint32_t new_state = 200;
    start = get_ns();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        changed = 0;
        smc_state_changed(ctx, &key, sizeof(key), &new_state, sizeof(new_state), &changed);
        sum += changed;
    }
    elapsed = get_ns() - start;
    ns_per_op = (double)elapsed / NUM_ITERATIONS;
    printf("state changed:        %.1f ns/op\n", ns_per_op);
    
    /* Benchmark: artifact lookup hit */
    size_t out_size = 0;
    uint8_t out_buf[256];
    start = get_ns();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        rc = smc_artifact_lookup(ctx, &hit_key, sizeof(hit_key), out_buf, sizeof(out_buf), &out_size);
        sum += (rc == SMC_OK ? 1 : 0);
    }
    elapsed = get_ns() - start;
    ns_per_op = (double)elapsed / NUM_ITERATIONS;
    printf("artifact lookup hit:  %.1f ns/op\n", ns_per_op);
    
    /* Benchmark: artifact lookup miss */
    uint32_t miss_key = 0x99999999;
    start = get_ns();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        rc = smc_artifact_lookup(ctx, &miss_key, sizeof(miss_key), out_buf, sizeof(out_buf), &out_size);
        sum += (rc == SMC_ERR_NOT_FOUND ? 1 : 0);
    }
    elapsed = get_ns() - start;
    ns_per_op = (double)elapsed / NUM_ITERATIONS;
    printf("artifact lookup miss: %.1f ns/op\n", ns_per_op);
    
    /* Benchmark: artifact store (same-key update) */
    start = get_ns();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        smc_artifact_store(ctx, &hit_key, sizeof(hit_key), blob, sizeof(blob));
    }
    elapsed = get_ns() - start;
    ns_per_op = (double)elapsed / NUM_ITERATIONS;
    printf("artifact update:      %.1f ns/op\n", ns_per_op);
    
    /* Prevent unused variable warnings */
    if (sum == 0) printf("Sum: %lu\n", (unsigned long)sum);
    
    smc_context_destroy(ctx);
    smc_shutdown();
    
    printf("\nSMC v2 artifact/state microbenchmark complete.\n");
    return 0;
}