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

/* Fixed-size slot for preallocated storage (v2.1) */
typedef struct {
    unsigned char *data;     /* Pointer to slot buffer */
    size_t data_size;      /* Size of slot buffer */
    int    occupied;         /* Is this slot in use? */
} smc_state_slot_t;

/* Dirty-state table */
typedef struct {
    smc_state_entry_t **entries;   /* Array of entry pointers */
    size_t entry_count;           /* Number of slots (max_entries) */
    size_t max_key_size;          /* Maximum allowed key size */
    size_t max_state_size;        /* Maximum allowed state size */
    size_t memory_budget;         /* Total bytes allocated */
    int    configured;            /* Has configure been called? */
    smc_state_stats_t *stats;     /* Pointer to stats (in context) */
    
    /* v2.1 preallocated storage - fixed slots */
    smc_state_slot_t *slots;      /* Array of fixed-size slots */
    int    use_preallocated;       /* 1 = use preallocated slots, 0 = malloc each */
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

/* -------------------------------------------------------------------------- */
/* Indexed-state tracking (ABI v2.1)                                            */
/* -------------------------------------------------------------------------- */

/* Indexed state table for dense integer-indexed state tracking */
typedef struct {
    unsigned char *states;    /* Contiguous state buffer: states[count * state_size] */
    uint8_t       *valid;    /* Validity flags: valid[count] (uint8_t to save space) */
    size_t         count;     /* Number of indexed slots */
    size_t         state_size; /* Size of each state record in bytes */
    int            configured; /* Has configure been called? */
    smc_state_indexed_stats_t *stats; /* Pointer to stats (in context) */
} smc_indexed_state_table_t;

/* Initialize an indexed state table with the given configuration. */
int smc_indexed_state_table_init(smc_indexed_state_table_t *table,
                                   const smc_state_indexed_config_t *config,
                                   smc_state_indexed_stats_t *stats);

/* Destroy an indexed state table, freeing all memory. */
void smc_indexed_state_table_destroy(smc_indexed_state_table_t *table);

/* Check if state changed for a given index. Returns SMC_OK, sets *out_changed. */
int smc_indexed_state_table_check(smc_indexed_state_table_t *table,
                                   uint32_t index,
                                   const void *state, size_t state_size,
                                   int *out_changed);

/* Clear all entries. */
int smc_indexed_state_table_clear(smc_indexed_state_table_t *table);

/* Process a batch of indexed states. Returns SMC_OK. */
int smc_indexed_state_table_diff_batch(smc_indexed_state_table_t *table,
                                         const void *states,
                                         size_t count,
                                         size_t stride,
                                         uint32_t *dirty_indices,
                                         size_t dirty_capacity,
                                         size_t *out_dirty_count);

/* Process a batch of indexed states described as multiple streams. Returns SMC_OK. */
int smc_indexed_state_table_diff_streams(smc_indexed_state_table_t *table,
                                            const smc_state_stream_t *streams,
                                            size_t stream_count,
                                            size_t record_count,
                                            uint32_t *dirty_indices,
                                            size_t dirty_capacity,
                                            size_t *out_dirty_count);

#endif /* SMC_STATE_H */
