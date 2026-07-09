/* examples/c/dirty_state_basic.c — dirty-state tracking usage */
/*
 * This example demonstrates the dirty-state API:
 *   - Configuring the state tracker
 *   - Checking if state has changed
 *   - Using this to skip work on unchanged state
 */

#include "smc.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    if (smc_init() != SMC_OK) {
        fprintf(stderr, "smc_init failed\n");
        return 1;
    }

    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) {
        fprintf(stderr, "smc_context_create failed\n");
        smc_shutdown();
        return 1;
    }

    smc_state_config_t config = {0}; /* Use defaults */
    int rc = smc_state_configure(ctx, &config);
    if (rc != SMC_OK) {
        fprintf(stderr, "smc_state_configure failed: %s\n", smc_error_string(rc));
        smc_context_destroy(ctx);
        smc_shutdown();
        return 1;
    }

    printf("State tracker configured\n");

    /* Simulate a cell that changes state over time */
    struct {
        uint32_t id;
        uint32_t glyph;
        uint32_t fg;
        uint32_t bg;
    } cell = { .id = 42, .glyph = 65, .fg = 0xFF0000FF, .bg = 0x00000000 };

    int total_checks = 10;
    int work_skipped = 0;

    for (int frame = 0; frame < total_checks; frame++) {
        int changed = 0;
        
        /* In a real renderer, we'd compare to previous state */
        rc = smc_state_changed(ctx, &cell.id, sizeof(cell.id), &cell, sizeof(cell), &changed);
        if (rc != SMC_OK) {
            fprintf(stderr, "smc_state_changed failed\n");
            continue;
        }

        if (changed) {
            printf("Frame %d: Cell %u state changed, doing work...\n", frame, cell.id);
        } else {
            printf("Frame %d: Cell %u unchanged, skipping work!\n", frame, cell.id);
            work_skipped++;
        }

        /* Simulate state change on frame 5 */
        if (frame == 5) {
            cell.glyph = 66; /* Change glyph */
            printf("  -> State changed! (glyph now %u)\n", cell.glyph);
        }
    }

    /* Show stats */
    smc_state_stats_t stats;
    smc_state_get_stats(ctx, &stats);
    printf("\nStats: checks=%llu, changed=%llu, unchanged=%llu\n",
           stats.checks, stats.changed, stats.unchanged);
    printf("Work skipped: %d / %d (%.1f%%)\n", work_skipped, total_checks, 
           100.0 * work_skipped / total_checks);

    smc_context_destroy(ctx);
    smc_shutdown();
    return 0;
}