/* smc_state.h — internal header for dirty-state tracking implementation
 *
 * This header is NOT installed. It provides the implementation details
 * for the dirty-state tracking subsystem used by smc_runtime_stub.c.
 */

#ifndef SMC_STATE_H
#define SMC_STATE_H

#include "smc.h"
#include <stddef.h>
#include <stdint.h>

/* Stats struct is defined in smc.h - use it directly */
/* Internal state entry - stored in a direct-mapped hash table */
typedef struct {
    uint32_t hash;               /* Full hash of the key (0 if empty) */
    size_t   key_size;           /* Actual key size in bytes */
    size_t   state_size;         /* Actual state size in bytes */
    unsigned char key_data[];      /* Key bytes (variable) */
    /* Followed by state bytes: key_data[key_size] ... state */
} smc_state_entry_t;

/* Dirty-state table */
typedef struct {
    smc_state_entry_t **entries;   /* Array of entry pointers */
    size_t entry_count;           /* Number of slots (max_entries) */
    size_t max_key_size;          /* Maximum allowed key size */
    size_t max_state_size;        /* Maximum allowed state size */
    size_t memory_budget;         /* Total bytes allocated */
    int    configured;            /* Has configure been called? */
    smc_state_stats_t *stats; /* Pointer to stats (in context) */
} smc_state_table_t;

/* Initialize a state table with the given configuration. */
int smc_state_table_init(smc_state_table_t *table,
                          const smc_state_config_t *config,
                          smc_state_stats_t *stats);

/* Destroy a state table, freeing all memory. */
void smc_state_table_destroy(smc_state_table_t *table);

/* Check if state changed. Returns SMC_OK, sets *out_changed. */
int smc_state_table_check(smc_state_table_t *table,
                           const void *key, size_t key_size,
                           const void *state, size_t state_size,
                           int *out_changed);

/* Clear all entries. */
int smc_state_table_clear(smc_state_table_t *table);

#endif /* SMC_STATE_H */