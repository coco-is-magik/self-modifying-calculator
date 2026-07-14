/* tests/benchmarks/benchmark_indexed_state.c — microbenchmark for SMC v2.1 indexed state API
 *
 * Measures operation costs for:
 *   - generic smc_state_changed() unchanged path
 *   - indexed smc_state_changed_index() unchanged path
 *   - batch indexed unchanged path
 *   - generic changed path
 *   - indexed changed path
 *   - batch changed path
 *   - fixed-size batch kernels vs generic reference loop
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

/* State sizes to evaluate for the fixed-size kernel matrix */
static const size_t MATRIX_SIZES[] = {1, 2, 4, 8, 16, 7, 12, 24, 32};
static const size_t MATRIX_SIZE_COUNT = sizeof(MATRIX_SIZES) / sizeof(MATRIX_SIZES[0]);

/* Change rates to evaluate (percentage of records changed) */
static const int CHANGE_RATES[] = {0, 1, 10, 50, 100};
static const size_t CHANGE_RATE_COUNT = sizeof(CHANGE_RATES) / sizeof(CHANGE_RATES[0]);

/* Get monotonic time in nanoseconds */
static uint64_t get_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* Fill a buffer with deterministic state records. */
static void fill_states(void *buffer, size_t count, size_t state_size, size_t stride) {
    unsigned char *base = (unsigned char *)buffer;
    for (size_t i = 0; i < count; i++) {
        unsigned char *slot = base + i * stride;
        memset(slot, 0, stride);
        if (state_size == 1) {
            slot[0] = (uint8_t)(i & 0xFF);
        } else if (state_size == 2) {
            uint16_t val = (uint16_t)i;
            memcpy(slot, &val, 2);
        } else if (state_size == 4) {
            uint32_t val = (uint32_t)i;
            memcpy(slot, &val, 4);
        } else if (state_size == 8) {
            uint64_t val = (uint64_t)i;
            memcpy(slot, &val, 8);
        } else if (state_size == 16) {
            uint64_t lo = (uint64_t)i;
            uint64_t hi = (uint64_t)(i + 1);
            memcpy(slot, &lo, 8);
            memcpy(slot + 8, &hi, 8);
        } else {
            /* Generic sizes: fill with deterministic byte pattern */
            for (size_t b = 0; b < state_size; b++) {
                slot[b] = (uint8_t)((i + b) & 0xFF);
            }
        }
    }
}

/* Modify a percentage of records in the buffer. */
static void modify_states(void *buffer, size_t count, size_t state_size, size_t stride, int percent) {
    unsigned char *base = (unsigned char *)buffer;
    for (size_t i = 0; i < count; i++) {
        if ((i * 100) / count < (size_t)percent) {
            unsigned char *slot = base + i * stride;
            for (size_t b = 0; b < state_size; b++) {
                slot[b] = (unsigned char)(slot[b] ^ 0xFF);
            }
        }
    }
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
    
#ifdef SMC_DISABLE_OPTIMIZED_STREAM_KERNELS
    printf("SMC indexed state benchmark (STREAM BASELINE: generic memcmp/memcpy, no short-circuit)\n");
#else
#ifdef SMC_DISABLE_FIXED_BATCH_KERNELS
    printf("SMC indexed state benchmark (BATCH BASELINE: generic-only, no fixed kernels)\n");
#else
    printf("SMC indexed state benchmark (OPTIMIZED)\n");
#endif
#endif
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
    
    /* ---------------------------------------------------------------------- */
    /* Fixed-size kernel matrix                                               */
    /* ---------------------------------------------------------------------- */
#ifdef SMC_DISABLE_FIXED_BATCH_KERNELS
    printf("\nFixed-size kernel matrix: GENERIC-ONLY BASELINE (records=%d)\n", NUM_RECORDS);
#else
    printf("\nFixed-size kernel matrix: FIXED KERNELS ENABLED (records=%d)\n", NUM_RECORDS);
#endif
    printf("state_size | change_rate | median_ns | total_ms | dirty_count | checks | changed | unchanged | stores | bytes_compared\n");
    printf("-----------|-------------|-----------|----------|-------------|--------|---------|-----------|--------|---------------\n");
    
    for (size_t s = 0; s < MATRIX_SIZE_COUNT; s++) {
        size_t state_size = MATRIX_SIZES[s];
        size_t stride = state_size;
        size_t buffer_size = NUM_RECORDS * stride;
        
        uint8_t *matrix_states = (uint8_t *)malloc(buffer_size);
        if (!matrix_states) {
            fprintf(stderr, "FAIL: out of memory for matrix states\n");
            return 1;
        }
        
        smc_context_t *matrix_ctx = smc_context_create(1);
        if (!matrix_ctx) {
            free(matrix_states);
            fprintf(stderr, "FAIL: context create for matrix\n");
            return 1;
        }
        
        smc_state_indexed_config_t matrix_config = {
            .count = NUM_RECORDS,
            .state_size = state_size,
            .memory_budget_bytes = 0
        };
        rc = smc_state_indexed_configure(matrix_ctx, &matrix_config);
        if (rc != SMC_OK) {
            free(matrix_states);
            smc_context_destroy(matrix_ctx);
            fprintf(stderr, "FAIL: indexed state configure for matrix size %zu\n", state_size);
            return 1;
        }
        
        for (size_t r = 0; r < CHANGE_RATE_COUNT; r++) {
            int change_rate = CHANGE_RATES[r];
            double pass_ns[3];
            
            for (int pass = 0; pass < 3; pass++) {
                /* Reset state and fill with deterministic values */
                smc_state_indexed_clear(matrix_ctx);
                fill_states(matrix_states, NUM_RECORDS, state_size, stride);
                
                /* First observation: all changed */
                size_t first_dirty = 0;
                smc_state_diff_indexed_batch(matrix_ctx, matrix_states, NUM_RECORDS, stride,
                                             NULL, 0, &first_dirty);
                
                /* Modify the requested percentage for steady-state observation.
                 * We will toggle these records each iteration so the change rate
                 * is sustained across the timed loop. */
                if (change_rate > 0) {
                    modify_states(matrix_states, NUM_RECORDS, state_size, stride, change_rate);
                }
                
                /* Benchmark public API (fixed kernel or generic fallback).
                 * Each iteration toggles the modified records so the requested
                 * percentage stays dirty, matching a steady-state changed workload. */
                const int MATRIX_ITERATIONS = 100;
                uint64_t kernel_start = get_ns();
                for (int iter = 0; iter < MATRIX_ITERATIONS; iter++) {
                    size_t dirty_count = 0;
                    smc_state_diff_indexed_batch(matrix_ctx, matrix_states, NUM_RECORDS, stride,
                                                 NULL, 0, &dirty_count);
                    if (change_rate > 0) {
                        modify_states(matrix_states, NUM_RECORDS, state_size, stride, change_rate);
                    }
                }
                uint64_t kernel_elapsed = get_ns() - kernel_start;
                pass_ns[pass] = (double)kernel_elapsed / (MATRIX_ITERATIONS * NUM_RECORDS);
            }
            
            /* Compute median of 3 passes */
            double median_ns = pass_ns[0];
            if (pass_ns[1] < median_ns) median_ns = pass_ns[1];
            if (pass_ns[2] < median_ns) median_ns = pass_ns[2];
            if (pass_ns[1] > pass_ns[0] && pass_ns[1] < pass_ns[2]) median_ns = pass_ns[1];
            if (pass_ns[2] > pass_ns[0] && pass_ns[2] < pass_ns[1]) median_ns = pass_ns[2];
            
            double total_ms = median_ns * NUM_RECORDS / 1000000.0;
            
            smc_state_indexed_stats_t stats;
            smc_state_indexed_get_stats(matrix_ctx, &stats);
            
            printf("%10zu | %11d%% | %9.1f | %8.3f | %11zu | %6llu | %7llu | %9llu | %6llu | %14llu\n",
                   state_size,
                   change_rate,
                   median_ns,
                   total_ms,
                   (size_t)(NUM_RECORDS * change_rate / 100),
                   (unsigned long long)stats.checks,
                   (unsigned long long)stats.changed,
                   (unsigned long long)stats.unchanged,
                   (unsigned long long)stats.stores,
                   (unsigned long long)stats.bytes_compared);
        }
        
        free(matrix_states);
        smc_context_destroy(matrix_ctx);
    }
    
    smc_shutdown();
    
    /* ---------------------------------------------------------------------- */
    /* Stream diff comparison (v2.2)                                          */
    /* ---------------------------------------------------------------------- */
    printf("\nStream diff comparison (records=%d)\n", NUM_RECORDS);
    printf("layout_type | change_rate | packed_ns/op | stream_ns/op | direct_ns/op | packed_ms | stream_ms | direct_ms\n");
    printf("------------|-------------|--------------|--------------|--------------|-----------|-----------|----------\n");
    
    /* Mixed 1+3+3 layout: glyph(1) + fg(3) + bg(3) = 7 bytes */
    {
        uint8_t *glyphs = (uint8_t *)malloc(NUM_RECORDS);
        uint8_t *fg = (uint8_t *)malloc(NUM_RECORDS * 3);
        uint8_t *bg = (uint8_t *)malloc(NUM_RECORDS * 3);
        uint64_t *packed = (uint64_t *)malloc(NUM_RECORDS * sizeof(uint64_t));
        if (glyphs && fg && bg && packed) {
            for (size_t i = 0; i < NUM_RECORDS; i++) {
                glyphs[i] = (uint8_t)(i & 0xFF);
                memset(&fg[i * 3], (uint8_t)((i + 1) & 0xFF), 3);
                memset(&bg[i * 3], (uint8_t)((i + 2) & 0xFF), 3);
            }
            
            smc_context_t *stream_ctx = smc_context_create(1);
            if (stream_ctx) {
                smc_state_indexed_config_t stream_config = {
                    .count = NUM_RECORDS,
                    .state_size = 7,
                    .memory_budget_bytes = 0
                };
                if (smc_state_indexed_configure(stream_ctx, &stream_config) == SMC_OK) {
                    for (size_t r = 0; r < CHANGE_RATE_COUNT; r++) {
                        int change_rate = CHANGE_RATES[r];
                        double pass_ns[3];
                        double packed_pass_ns[3];
                        double direct_pass_ns[3];
                        
                        for (int pass = 0; pass < 3; pass++) {
                            smc_state_indexed_clear(stream_ctx);
                            
                            /* First observation with stream diff */
                            size_t first_dirty = 0;
                            smc_state_stream_t streams[3] = {
                                {glyphs, 1, 1},
                                {fg, 3, 3},
                                {bg, 3, 3},
                            };
                            smc_state_diff_indexed_streams(stream_ctx, streams, 3, NUM_RECORDS,
                                                            NULL, 0, &first_dirty);
                            
                            /* Apply change rate */
                            if (change_rate > 0) {
                                size_t changed = (NUM_RECORDS * (size_t)change_rate) / 100;
                                for (size_t i = 0; i < changed; i++) {
                                    glyphs[i] = (uint8_t)(glyphs[i] ^ 0xFF);
                                }
                            }
                            
                            /* Benchmark stream diff */
                            uint64_t stream_start = get_ns();
                            for (int iter = 0; iter < 100; iter++) {
                                size_t dirty_count = 0;
                                smc_state_stream_t streams[3] = {
                                    {glyphs, 1, 1},
                                    {fg, 3, 3},
                                    {bg, 3, 3},
                                };
                                smc_state_diff_indexed_streams(stream_ctx, streams, 3, NUM_RECORDS,
                                                                NULL, 0, &dirty_count);
                                if (change_rate > 0) {
                                    size_t changed = (NUM_RECORDS * (size_t)change_rate) / 100;
                                    for (size_t i = 0; i < changed; i++) {
                                        glyphs[i] = (uint8_t)(glyphs[i] ^ 0xFF);
                                    }
                                }
                            }
                            uint64_t stream_elapsed = get_ns() - stream_start;
                            pass_ns[pass] = (double)stream_elapsed / (100.0 * NUM_RECORDS);
                            
                            /* Benchmark packed batch including packing cost */
                            smc_state_indexed_clear(stream_ctx);
                            uint64_t packed_start = get_ns();
                            for (int iter = 0; iter < 100; iter++) {
                                /* Packing loop cost included */
                                for (size_t i = 0; i < NUM_RECORDS; i++) {
                                    packed[i] =
                                        ((uint64_t)glyphs[i] << 0)  |
                                        ((uint64_t)fg[i * 3 + 0] << 8)  |
                                        ((uint64_t)fg[i * 3 + 1] << 16) |
                                        ((uint64_t)fg[i * 3 + 2] << 24) |
                                        ((uint64_t)bg[i * 3 + 0] << 32) |
                                        ((uint64_t)bg[i * 3 + 1] << 40) |
                                        ((uint64_t)bg[i * 3 + 2] << 48);
                                }
                                size_t dirty_count = 0;
                                smc_state_diff_indexed_batch(stream_ctx, packed, NUM_RECORDS, 8,
                                                              NULL, 0, &dirty_count);
                                if (change_rate > 0) {
                                    size_t changed = (NUM_RECORDS * (size_t)change_rate) / 100;
                                    for (size_t i = 0; i < changed; i++) {
                                        glyphs[i] = (uint8_t)(glyphs[i] ^ 0xFF);
                                    }
                                }
                            }
                            uint64_t packed_elapsed = get_ns() - packed_start;
                            packed_pass_ns[pass] = (double)packed_elapsed / (100.0 * NUM_RECORDS);
                            
                            /* Benchmark direct comparison loop baseline */
                            uint8_t *snapshot = (uint8_t *)malloc(NUM_RECORDS * 7);
                            if (snapshot) {
                                for (size_t i = 0; i < NUM_RECORDS; i++) {
                                    snapshot[i * 7 + 0] = glyphs[i];
                                    memcpy(&snapshot[i * 7 + 1], &fg[i * 3], 3);
                                    memcpy(&snapshot[i * 7 + 4], &bg[i * 3], 3);
                                }
                                uint64_t direct_start = get_ns();
                                for (int iter = 0; iter < 100; iter++) {
                                    size_t dirty_count = 0;
                                    for (size_t i = 0; i < NUM_RECORDS; i++) {
                                        int changed = 0;
                                        if (snapshot[i * 7 + 0] != glyphs[i]) changed = 1;
                                        if (memcmp(&snapshot[i * 7 + 1], &fg[i * 3], 3) != 0) changed = 1;
                                        if (memcmp(&snapshot[i * 7 + 4], &bg[i * 3], 3) != 0) changed = 1;
                                        if (changed) {
                                            snapshot[i * 7 + 0] = glyphs[i];
                                            memcpy(&snapshot[i * 7 + 1], &fg[i * 3], 3);
                                            memcpy(&snapshot[i * 7 + 4], &bg[i * 3], 3);
                                            dirty_count++;
                                        }
                                    }
                                    if (change_rate > 0) {
                                        size_t changed = (NUM_RECORDS * (size_t)change_rate) / 100;
                                        for (size_t i = 0; i < changed; i++) {
                                            glyphs[i] = (uint8_t)(glyphs[i] ^ 0xFF);
                                        }
                                    }
                                }
                                uint64_t direct_elapsed = get_ns() - direct_start;
                                direct_pass_ns[pass] = (double)direct_elapsed / (100.0 * NUM_RECORDS);
                                free(snapshot);
                            } else {
                                direct_pass_ns[pass] = 0.0;
                            }
                        }
                        
                        /* Median of 3 */
                        double median_stream = pass_ns[0];
                        if (pass_ns[1] < median_stream) median_stream = pass_ns[1];
                        if (pass_ns[2] < median_stream) median_stream = pass_ns[2];
                        if (pass_ns[1] > pass_ns[0] && pass_ns[1] < pass_ns[2]) median_stream = pass_ns[1];
                        if (pass_ns[2] > pass_ns[0] && pass_ns[2] < pass_ns[1]) median_stream = pass_ns[2];
                        
                        double median_packed = packed_pass_ns[0];
                        if (packed_pass_ns[1] < median_packed) median_packed = packed_pass_ns[1];
                        if (packed_pass_ns[2] < median_packed) median_packed = packed_pass_ns[2];
                        if (packed_pass_ns[1] > packed_pass_ns[0] && packed_pass_ns[1] < packed_pass_ns[2]) median_packed = packed_pass_ns[1];
                        if (packed_pass_ns[2] > packed_pass_ns[0] && packed_pass_ns[2] < packed_pass_ns[1]) median_packed = packed_pass_ns[2];
                        
                        double median_direct = direct_pass_ns[0];
                        if (direct_pass_ns[1] < median_direct) median_direct = direct_pass_ns[1];
                        if (direct_pass_ns[2] < median_direct) median_direct = direct_pass_ns[2];
                        if (direct_pass_ns[1] > direct_pass_ns[0] && direct_pass_ns[1] < direct_pass_ns[2]) median_direct = direct_pass_ns[1];
                        if (direct_pass_ns[2] > direct_pass_ns[0] && direct_pass_ns[2] < direct_pass_ns[1]) median_direct = direct_pass_ns[2];
                        
                        printf("mixed_1_3_3 | %11d%% | %12.1f | %12.1f | %12.1f | %9.3f | %9.3f | %9.3f\n",
                               change_rate,
                               median_packed,
                               median_stream,
                               median_direct,
                               median_packed * NUM_RECORDS / 1000000.0,
                               median_stream * NUM_RECORDS / 1000000.0,
                               median_direct * NUM_RECORDS / 1000000.0);
                    }
                }
                smc_context_destroy(stream_ctx);
            }
        }
        free(glyphs);
        free(fg);
        free(bg);
        free(packed);
    }
    
    printf("\nSMC indexed state benchmark complete.\n");
    (void)sum;
    return 0;
}
