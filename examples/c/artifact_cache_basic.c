/* examples/c/artifact_cache_basic.c — basic artifact cache usage */
/*
 * This example demonstrates the artifact cache API:
 *   - Configuring the cache with custom limits
 *   - Storing and retrieving binary artifacts by opaque keys
 *   - Checking stats for cache performance
 */

#include "smc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    if (smc_init() != SMC_OK) {
        fprintf(stderr, "smc_init failed\n");
        return 1;
    }

    /* Create a context and configure artifact cache */
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) {
        fprintf(stderr, "smc_context_create failed\n");
        smc_shutdown();
        return 1;
    }

    smc_artifact_config_t config = {
        .max_entries = 256,
        .max_key_size = 16,
        .max_value_size = 64,
        .memory_budget_bytes = 0
    };

    int rc = smc_artifact_configure(ctx, &config);
    if (rc != SMC_OK) {
        fprintf(stderr, "smc_artifact_configure failed: %s\n", smc_error_string(rc));
        smc_context_destroy(ctx);
        smc_shutdown();
        return 1;
    }

    printf("Artifact cache configured: max_entries=%zu, max_key_size=%zu, max_value_size=%zu\n",
           config.max_entries, config.max_key_size, config.max_value_size);

    /* Store an artifact */
    uint64_t key = 12345;
    char value[] = "Hello, artifact cache!";
    
    rc = smc_artifact_store(ctx, &key, sizeof(key), value, sizeof(value));
    if (rc != SMC_OK) {
        fprintf(stderr, "smc_artifact_store failed: %s\n", smc_error_string(rc));
    } else {
        printf("Stored artifact with key %llu, value: %s\n", (unsigned long long)key, value);
    }

    /* Retrieve the artifact */
    char out_value[64] = {0};
    size_t out_size = 0;
    
    rc = smc_artifact_lookup(ctx, &key, sizeof(key), out_value, sizeof(out_value), &out_size);
    if (rc == SMC_OK) {
        printf("Retrieved artifact: \"%s\" (size=%zu)\n", out_value, out_size);
    } else {
        printf("Artifact not found (miss)\n");
    }

    /* Show stats */
    smc_artifact_stats_t stats;
    smc_artifact_get_stats(ctx, &stats);
    printf("Stats: lookups=%llu, hits=%llu, misses=%llu, stores=%llu, evictions=%llu\n",
           stats.lookups, stats.hits, stats.misses, stats.stores, stats.evictions);

    smc_context_destroy(ctx);
    smc_shutdown();
    return 0;
}