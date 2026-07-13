/* smc_state.c — dirty-state tracking implementation for SMC
 *
 * This file implements a direct-mapped hash table for tracking state
 * changes by opaque binary keys.
 * v2.1: Uses preallocated fixed-size slots to avoid per-store malloc.
 */

#include "smc_state.h"
#include <stdlib.h>
#include <string.h>

/* FNV-1a 32-bit hash - same as artifact cache */
static uint32_t smc_byte_hash(const void *data, size_t size) {
    const unsigned char *bytes = (const unsigned char *)data;
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

int smc_state_table_init(smc_state_table_t *table,
                          const smc_state_config_t *config,
                          smc_state_stats_t *stats) {
    if (!table || !config) {
        return SMC_ERR_INVALID;
    }
    
    size_t max_entries = config->max_entries;
    if (max_entries == 0) max_entries = SMC_STATE_DEFAULT_MAX_ENTRIES;
    
    size_t max_key_size = config->max_key_size;
    if (max_key_size == 0) max_key_size = SMC_STATE_DEFAULT_MAX_KEY_SIZE;
    
    size_t max_state_size = config->max_state_size;
    if (max_state_size == 0) max_state_size = SMC_STATE_DEFAULT_MAX_STATE_SIZE;
    
    /* v2.1: Allocate fixed slots (one per entry) - each slot holds max_key + max_state */
    size_t slot_size = sizeof(smc_state_entry_t) + max_key_size + max_state_size;
    size_t total_buffer_size = max_entries * slot_size;
    
    /* Enforce user-provided memory budget if specified */
    size_t budget = config->memory_budget_bytes;
    if (budget != 0 && total_buffer_size > budget) {
        return SMC_ERR_CAPACITY;
    }
    
    /* Allocate slots array */
    table->slots = (smc_state_slot_t *)calloc(max_entries, sizeof(smc_state_slot_t));
    if (!table->slots) {
        return SMC_ERR_INIT;
    }
    
    /* Allocate a single buffer for all slots */
    unsigned char *slot_buffer = (unsigned char *)malloc(total_buffer_size);
    if (!slot_buffer) {
        free(table->slots);
        table->slots = NULL;
        return SMC_ERR_INIT;
    }
    
    /* Initialize slots to point into the buffer */
    for (size_t i = 0; i < max_entries; i++) {
        table->slots[i].data = slot_buffer + i * slot_size;
        table->slots[i].data_size = slot_size;
        table->slots[i].occupied = 0;
    }
    
    /* Allocate entry pointer array */
    table->entries = (smc_state_entry_t **)calloc(max_entries, sizeof(smc_state_entry_t *));
    if (!table->entries) {
        free(slot_buffer);
        free(table->slots);
        table->slots = NULL;
        return SMC_ERR_INIT;
    }
    
    table->entry_count = max_entries;
    table->max_key_size = max_key_size;
    table->max_state_size = max_state_size;
    table->memory_budget = total_buffer_size;
    table->use_preallocated = 1; /* v2.1 always uses preallocated fixed slots */
    table->configured = 1;
    table->stats = stats;
    
    /* Clear stats if provided */
    if (stats) {
        memset(stats, 0, sizeof(*stats));
    }
    
    return SMC_OK;
}

void smc_state_table_destroy(smc_state_table_t *table) {
    if (!table) return;
    
    /* Free slots buffer if we allocated it */
    if (table->slots) {
        if (table->slots[0].data) {
            free(table->slots[0].data); /* The whole buffer starts at slots[0].data */
        }
        free(table->slots);
        table->slots = NULL;
    }
    
    /* Free entry pointer array */
    if (table->entries) {
        free(table->entries);
        table->entries = NULL;
    }
    
    table->configured = 0;
    table->entry_count = 0;
    table->stats = NULL;
}

int smc_state_table_check(smc_state_table_t *table,
                          const void *key, size_t key_size,
                          const void *state, size_t state_size,
                          int *out_changed) {
    if (!table || !table->configured) {
        return SMC_ERR_INIT;
    }
    /* Note: state can be NULL only if state_size is 0 - allowing zero-size states */
    if (!key || key_size == 0 || !out_changed) {
        return SMC_ERR_INVALID;
    }
    if (key_size > table->max_key_size) {
        return SMC_ERR_SIZE;
    }
    if (state_size > table->max_state_size) {
        return SMC_ERR_SIZE;
    }
    
    if (table->stats) {
        table->stats->checks++;
        table->stats->bytes_compared += state_size;
    }
    
    uint32_t hash = smc_byte_hash(key, key_size);
    size_t index = hash % table->entry_count;
    smc_state_entry_t *entry = table->entries[index];
    
    /* Check for existing entry with same key */
    if (entry && entry->hash == hash && entry->key_size == key_size &&
        memcmp(entry->key_data, key, key_size) == 0) {
        /* Key exists - compare state */
        void *stored_state = entry->key_data + key_size;
        if (entry->state_size == state_size &&
            (state_size == 0 || memcmp(stored_state, state, state_size) == 0)) {
            /* Unchanged */
            *out_changed = 0;
            if (table->stats) {
                table->stats->unchanged++;
            }
            return SMC_OK;
        }
        
        /* Changed - update state */
        *out_changed = 1;
        if (table->stats) {
            table->stats->changed++;
            table->stats->stores++;
        }
        
        entry->hash = hash;
        entry->key_size = key_size;
        entry->state_size = state_size;
        memcpy(entry->key_data, key, key_size);
        if (state && state_size > 0) {
            memcpy(entry->key_data + key_size, state, state_size);
        }
        return SMC_OK;
    }
    
    /* First observation of this key - store state */
    *out_changed = 1;
    if (table->stats) {
        table->stats->changed++;
        table->stats->stores++;
    }
    
    /* v2.1: Use preallocated slot */
    smc_state_entry_t *new_entry = (smc_state_entry_t *)table->slots[index].data;
    
    /* Track eviction on collision */
    if (entry && table->stats) {
        table->stats->evictions++;
    }
    
    new_entry->hash = hash;
    new_entry->key_size = key_size;
    new_entry->state_size = state_size;
    memcpy(new_entry->key_data, key, key_size);
    if (state && state_size > 0) {
        memcpy(new_entry->key_data + key_size, state, state_size);
    }
    table->slots[index].occupied = 1;
    
    table->entries[index] = new_entry;
    return SMC_OK;
}

int smc_state_table_clear(smc_state_table_t *table) {
    if (!table || !table->configured) {
        return SMC_ERR_INIT;
    }
    
    /* v2.1: Clear all slot occupancy flags */
    for (size_t i = 0; i < table->entry_count; i++) {
        table->slots[i].occupied = 0;
        table->entries[i] = NULL;
    }
    
    return SMC_OK;
}

/* -------------------------------------------------------------------------- */
/* Indexed-state tracking implementation (ABI v2.1)                             */
/* -------------------------------------------------------------------------- */

int smc_indexed_state_table_init(smc_indexed_state_table_t *table,
                                   const smc_state_indexed_config_t *config,
                                   smc_state_indexed_stats_t *stats) {
    if (!table || !config) {
        return SMC_ERR_INVALID;
    }
    
    size_t count = config->count;
    if (count == 0) {
        return SMC_ERR_INVALID; /* Must specify count for indexed table */
    }
    
    size_t state_size = config->state_size; /* Can be 0 for zero-size states */
    
    /* Calculate total memory: states buffer + validity array */
    size_t states_buffer_size = count * state_size;
    size_t valid_array_size = count * sizeof(uint8_t);
    size_t total_buffer_size = states_buffer_size + valid_array_size;
    
    /* Enforce user-provided memory budget if specified */
    size_t budget = config->memory_budget_bytes;
    if (budget != 0 && total_buffer_size > budget) {
        return SMC_ERR_CAPACITY;
    }
    
    /* Allocate a single contiguous buffer for states + valid array */
    unsigned char *buffer = (unsigned char *)malloc(total_buffer_size);
    if (!buffer) {
        return SMC_ERR_INIT;
    }
    
    table->states = buffer;
    table->valid = (uint8_t *)(buffer + states_buffer_size);
    table->count = count;
    table->state_size = state_size;
    table->configured = 1;
    table->stats = stats;
    
    /* Clear stats if provided */
    if (stats) {
        memset(stats, 0, sizeof(*stats));
    }
    
    /* Clear validity array */
    memset(table->valid, 0, valid_array_size);
    
    return SMC_OK;
}

void smc_indexed_state_table_destroy(smc_indexed_state_table_t *table) {
    if (!table) return;
    
    if (table->states) {
        free(table->states);
        table->states = NULL;
    }
    
    table->valid = NULL;
    table->count = 0;
    table->state_size = 0;
    table->configured = 0;
    table->stats = NULL;
}

int smc_indexed_state_table_check(smc_indexed_state_table_t *table,
                                   uint32_t index,
                                   const void *state,
                                   size_t state_size,
                                   int *out_changed) {
    if (!table || !table->configured) {
        return SMC_ERR_INIT;
    }
    if (!out_changed) {
        return SMC_ERR_INVALID;
    }
    /* For zero-size states, we only track validity, not compare bytes */
    if (state_size != table->state_size) {
        return SMC_ERR_SIZE;
    }
    
    if (index >= table->count) {
        if (table->stats) {
            table->stats->out_of_range++;
        }
        return SMC_ERR_SIZE;
    }
    
    if (table->stats) {
        table->stats->checks++;
        table->stats->bytes_compared += state_size;
    }
    
    unsigned char *slot = table->states + (size_t)index * table->state_size;
    
    if (!table->valid[index]) {
        /* First observation - store state */
        if (state_size > 0 && state) {
            memcpy(slot, state, state_size);
        }
        table->valid[index] = 1;
        *out_changed = 1;
        if (table->stats) {
            table->stats->changed++;
            table->stats->stores++;
        }
        return SMC_OK;
    }
    
    /* Slot already valid - compare state */
    if (state_size == 0 || memcmp(slot, state, state_size) == 0) {
        /* State unchanged */
        *out_changed = 0;
        if (table->stats) {
            table->stats->unchanged++;
        }
        return SMC_OK;
    }
    
    /* State changed - update */
    if (state_size > 0 && state) {
        memcpy(slot, state, state_size);
    }
    *out_changed = 1;
    if (table->stats) {
        table->stats->changed++;
        table->stats->stores++;
    }
    return SMC_OK;
}

int smc_indexed_state_table_clear(smc_indexed_state_table_t *table) {
    if (!table || !table->configured) {
        return SMC_ERR_INIT;
    }
    
    /* Clear all validity flags */
    for (size_t i = 0; i < table->count; i++) {
        table->valid[i] = 0;
    }
    
    if (table->stats) {
        table->stats->clears++;
    }
    
    return SMC_OK;
}

int smc_indexed_state_table_diff_batch(smc_indexed_state_table_t *table,
                                        const void *states,
                                        size_t count,
                                        size_t stride,
                                        uint32_t *dirty_indices,
                                        size_t dirty_capacity,
                                        size_t *out_dirty_count) {
    if (!table || !table->configured) {
        return SMC_ERR_INIT;
    }
    if (!out_dirty_count) {
        return SMC_ERR_INVALID;
    }
    if (stride == 0) {
        return SMC_ERR_INVALID;
    }
    if (stride < table->state_size) {
        return SMC_ERR_SIZE;
    }
    if (dirty_indices == NULL && dirty_capacity > 0) {
        return SMC_ERR_INVALID;
    }
    if (count > table->count) {
        return SMC_ERR_SIZE;
    }
    if (count > 0 && table->state_size > 0 && states == NULL) {
        return SMC_ERR_INVALID;
    }
    
    size_t changed_count = 0;
    size_t write_pos = 0;
    
    for (size_t i = 0; i < count; i++) {
        /* Calculate pointer to state record i */
        const unsigned char *state_ptr = (const unsigned char *)states + i * stride;
        
        /* Check this index */
        unsigned char *slot = table->states + i * table->state_size;
        int is_changed = 1;
        
        if (table->valid[i]) {
            /* Already seen - compare */
            if (table->state_size == 0 || memcmp(slot, state_ptr, table->state_size) == 0) {
                is_changed = 0;
            } else {
                /* Update state */
                if (table->state_size > 0) {
                    memcpy(slot, state_ptr, table->state_size);
                }
            }
        } else {
            /* First observation - store state */
            if (table->state_size > 0) {
                memcpy(slot, state_ptr, table->state_size);
            }
            table->valid[i] = 1;
        }
        
        if (is_changed) {
            changed_count++;
            if (write_pos < dirty_capacity && dirty_indices != NULL) {
                dirty_indices[write_pos++] = (uint32_t)i;
            }
        }
    }
    
    if (table->stats) {
        table->stats->checks += count;
        table->stats->bytes_compared += count * table->state_size;
        table->stats->changed += changed_count;
        table->stats->unchanged += count - changed_count;
        table->stats->stores += changed_count;
    }
    
    *out_dirty_count = changed_count;
    return SMC_OK;
}