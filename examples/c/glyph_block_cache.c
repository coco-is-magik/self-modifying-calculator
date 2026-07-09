/* examples/c/glyph_block_cache.c — combined artifact + state usage pattern
 *
 * This example demonstrates the renderer pattern from the design:
 *   1. Use state_changed to skip unchanged cells
 *   2. On changed cells, use artifact_lookup to reuse rasterized glyphs
 *   3. On miss, rasterize and store the result for reuse
 */

#include "smc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Simulated glyph block (64 uint32_t pixels = 256 bytes) */
typedef struct {
    uint32_t pixels[64];
} GlyphBlock;

/* Simulated cell state */
typedef struct {
    uint32_t id;
    uint32_t glyph_id;
    uint32_t fg_rgba;
    uint32_t bg_rgba;
} CellState;

/* Simulated rasterization (expensive operation) */
static void rasterize_glyph(uint32_t glyph_id, uint32_t fg, uint32_t bg, GlyphBlock *out) {
    /* Fill with a pattern based on glyph_id - simulate work */
    for (int i = 0; i < 64; i++) {
        out->pixels[i] = (glyph_id << 24) | (fg & 0x00FFFFFF) | (bg & 0x00FFFFFF);
    }
}

/* Simulated blit (render to screen) */
static void blit_block(const GlyphBlock *block) {
    /* In a real renderer, this would draw pixels */
    (void)block;
}

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

    /* Configure both caches */
    smc_artifact_config_t art_config = {
        .max_entries = 1024,
        .max_key_size = 16,
        .max_value_size = sizeof(GlyphBlock),
        .memory_budget_bytes = 0
    };
    (void)smc_artifact_configure(ctx, &art_config);

    smc_state_config_t state_config = {0};
    (void)smc_state_configure(ctx, &state_config);

    printf("Caches configured\n");

    /* Simulate a render loop with 100 cells */
    CellState cells[100];
    for (int i = 0; i < 100; i++) {
        cells[i] = (CellState){
            .id = (uint32_t)i,
            .glyph_id = (uint32_t)(i % 10), /* Only 10 unique glyphs - high reuse */
            .fg_rgba = 0xFF0000FF,
            .bg_rgba = 0x00000000
        };
    }

    int total_cells = 100;
    int skipped_state = 0;
    int cache_hits = 0;
    int rasterizations = 0;

    /* Simulate 3 frames */
    for (int frame = 0; frame < 3; frame++) {
        printf("\n--- Frame %d ---\n", frame);
        
        /* Change some cells each frame */
        if (frame == 1) {
            cells[50].glyph_id = 7;  /* Modify cell 50 */
            cells[75].fg_rgba = 0x00FF00FF; /* Modify cell 75 */
        }

        for (int i = 0; i < total_cells; i++) {
            CellState *cell = &cells[i];
            int changed = 0;

            /* Step 1: Check if state changed */
            int rc = smc_state_changed(ctx, &cell->id, sizeof(cell->id), 
                                        cell, sizeof(*cell), &changed);
            if (rc != SMC_OK || changed == 0) {
                skipped_state++;
                continue; /* Skip work for unchanged cell */
            }

            /* Step 2: Try artifact cache */
            /* Compose a glyph key from the visual properties */
            struct {
                uint32_t glyph_id;
                uint32_t fg_rgba;
                uint32_t bg_rgba;
            } glyph_key = { cell->glyph_id, cell->fg_rgba, cell->bg_rgba };

            GlyphBlock block;
            rc = smc_artifact_lookup(ctx, &glyph_key, sizeof(glyph_key),
                                      &block, sizeof(block), NULL);
            if (rc == SMC_OK) {
                cache_hits++;
                blit_block(&block);
            } else {
                /* Step 3: Miss - rasterize and cache */
                rasterize_glyph(cell->glyph_id, cell->fg_rgba, cell->bg_rgba, &block);
                (void)smc_artifact_store(ctx, &glyph_key, sizeof(glyph_key),
                                          &block, sizeof(block));
                blit_block(&block);
                rasterizations++;
            }
        }
    }

    /* Show stats */
    smc_state_stats_t sstats;
    smc_artifact_stats_t astats;
    smc_state_get_stats(ctx, &sstats);
    smc_artifact_get_stats(ctx, &astats);

    printf("\n--- Summary ---\n");
    printf("State checks: %llu, unchanged: %llu\n", sstats.checks, sstats.unchanged);
    printf("Artifacts: lookups=%llu, hits=%llu, misses=%llu\n",
           astats.lookups, astats.hits, astats.misses);
    printf("Total cells skipped (unchanged): %d\n", skipped_state);
    printf("Total rasterizations (on cache misses): %d\n", rasterizations);

    smc_context_destroy(ctx);
    smc_shutdown();
    return 0;
}