/* smc_artifact.h — internal header for artifact cache implementation
 *
 * This header is NOT installed. It provides the implementation details
 * for the artifact cache subsystem used by smc_runtime_stub.c.
 */

#ifndef SMC_ARTIFACT_H
#define SMC_ARTIFACT_H

#include "smc.h"
#include <stddef.h>
#include <stdint.h>

/* Stats struct is defined in smc.h - use it directly */
/* Internal artifact entry - stored in a direct-mapped hash table */
typedef struct {
    uint32_t hash;          /* Full hash of the key (0 if empty) */
    size_t   key_size;      /* Actual key size in bytes */
    size_t   value_size;    /* Actual value size in bytes */
    unsigned char key_data[]; /* Key bytes (variable) */
    /* Followed by value bytes: key_data[key_size] ... value */
} smc_artifact_entry_t;

/* Artifact cache table */
typedef struct {
    smc_artifact_entry_t **entries;  /* Array of entry pointers */
    size_t entry_count;              /* Number of slots (max_entries) */
    size_t max_key_size;             /* Maximum allowed key size */
    size_t max_value_size;             /* Maximum allowed value size */
    size_t memory_budget;            /* Total bytes allocated */
    int    configured;               /* Has configure been called? */
    smc_artifact_stats_t *stats; /* Pointer to stats (in context) */
} smc_artifact_table_t;

/* Initialize an artifact table with the given configuration.
 * Returns 0 on success, negative error code on failure.
 * Memory is allocated for the entire table at this point. */
int smc_artifact_table_init(smc_artifact_table_t *table,
                             const smc_artifact_config_t *config,
                             smc_artifact_stats_t *stats);

/* Destroy an artifact table, freeing all memory. */
void smc_artifact_table_destroy(smc_artifact_table_t *table);

/* Lookup an artifact. Returns SMC_OK on hit, SMC_ERR_NOT_FOUND on miss,
 * SMC_ERR_SIZE if key/value exceeds limits. */
int smc_artifact_table_lookup(smc_artifact_table_t *table,
                               const void *key, size_t key_size,
                               void *out_value, size_t value_capacity,
                               size_t *out_value_size);

/* Store an artifact. Returns SMC_ERR_SIZE on limit violation. */
int smc_artifact_table_store(smc_artifact_table_t *table,
                              const void *key, size_t key_size,
                              const void *value, size_t value_size);

/* Remove an artifact by key. */
int smc_artifact_table_remove(smc_artifact_table_t *table,
                               const void *key, size_t key_size);

/* Clear all entries. */
int smc_artifact_table_clear(smc_artifact_table_t *table);

#endif /* SMC_ARTIFACT_H */