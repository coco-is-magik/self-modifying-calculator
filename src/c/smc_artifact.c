/* smc_artifact.c — artifact cache implementation for SMC
 *
 * This file implements a direct-mapped hash table for caching arbitrary
 * binary artifacts by opaque binary keys.
 * v2.1: Uses preallocated fixed-size slots to avoid per-store malloc.
 */

#include "smc_artifact.h"
#include <stdlib.h>
#include <string.h>

/* FNV-1a 32-bit hash for byte sequences */
static uint32_t smc_byte_hash(const void *data, size_t size) {
    const unsigned char *bytes = (const unsigned char *)data;
    uint32_t hash = 2166136261u; /* FNV offset basis */
    for (size_t i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= 16777619u; /* FNV prime */
    }
    return hash;
}

int smc_artifact_table_init(smc_artifact_table_t *table,
                             const smc_artifact_config_t *config,
                             smc_artifact_stats_t *stats) {
    if (!table || !config) {
        return SMC_ERR_INVALID;
    }
    
    size_t max_entries = config->max_entries;
    if (max_entries == 0) max_entries = SMC_ARTIFACT_DEFAULT_MAX_ENTRIES;
    
    size_t max_key_size = config->max_key_size;
    if (max_key_size == 0) max_key_size = SMC_ARTIFACT_DEFAULT_MAX_KEY_SIZE;
    
    size_t max_value_size = config->max_value_size;
    if (max_value_size == 0) max_value_size = SMC_ARTIFACT_DEFAULT_MAX_VALUE_SIZE;
    
    /* v2.1: Allocate fixed slots (one per entry) - each slot holds max_key + max_value */
    size_t slot_size = sizeof(smc_artifact_entry_t) + max_key_size + max_value_size;
    size_t total_buffer_size = max_entries * slot_size;
    
    /* Enforce user-provided memory budget if specified */
    size_t budget = config->memory_budget_bytes;
    if (budget != 0 && total_buffer_size > budget) {
        return SMC_ERR_CAPACITY;
    }
    
    /* Allocate slots array */
    table->slots = (smc_artifact_slot_t *)calloc(max_entries, sizeof(smc_artifact_slot_t));
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
    table->entries = (smc_artifact_entry_t **)calloc(max_entries, sizeof(smc_artifact_entry_t *));
    if (!table->entries) {
        free(slot_buffer);
        free(table->slots);
        table->slots = NULL;
        return SMC_ERR_INIT;
    }
    
    table->entry_count = max_entries;
    table->max_key_size = max_key_size;
    table->max_value_size = max_value_size;
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

void smc_artifact_table_destroy(smc_artifact_table_t *table) {
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

int smc_artifact_table_lookup(smc_artifact_table_t *table,
                               const void *key, size_t key_size,
                               void *out_value, size_t value_capacity,
                               size_t *out_value_size) {
    if (!table || !table->configured) {
        return SMC_ERR_INIT;
    }
    if (!key || key_size == 0 || !out_value) {
        return SMC_ERR_INVALID;
    }
    if (key_size > table->max_key_size) {
        return SMC_ERR_SIZE;
    }
    
    if (table->stats) {
        table->stats->lookups++;
    }
    
    uint32_t hash = smc_byte_hash(key, key_size);
    size_t index = hash % table->entry_count;
    smc_artifact_entry_t *entry = table->entries[index];
    
    /* Check for hit */
    if (entry && entry->hash == hash && entry->key_size == key_size &&
        memcmp(entry->key_data, key, key_size) == 0) {
        if (table->stats) {
            table->stats->hits++;
            table->stats->bytes_returned += entry->value_size;
        }
        
        if (value_capacity < entry->value_size) {
            if (out_value_size) {
                *out_value_size = entry->value_size;
            }
            return SMC_ERR_SIZE;
        }
        
        void *value_ptr = entry->key_data + key_size;
        memcpy(out_value, value_ptr, entry->value_size);
        if (out_value_size) {
            *out_value_size = entry->value_size;
        }
        return SMC_OK;
    }
    
    /* Miss */
    if (table->stats) {
        table->stats->misses++;
    }
    
    if (out_value_size) {
        *out_value_size = 0;
    }
    return SMC_ERR_NOT_FOUND;
}

int smc_artifact_table_store(smc_artifact_table_t *table,
                              const void *key, size_t key_size,
                              const void *value, size_t value_size) {
    if (!table || !table->configured) {
        return SMC_ERR_INIT;
    }
    if (!key || key_size == 0) {
        return SMC_ERR_INVALID;
    }
    if (key_size > table->max_key_size) {
        return SMC_ERR_SIZE;
    }
    if (value_size > table->max_value_size) {
        return SMC_ERR_SIZE;
    }
    
    if (table->stats) {
        table->stats->stores++;
        table->stats->bytes_stored += value_size;
    }
    
    uint32_t hash = smc_byte_hash(key, key_size);
    size_t index = hash % table->entry_count;
    smc_artifact_entry_t *old_entry = table->entries[index];
    
    /* Track if this is an update vs. new entry */
    int is_update = (old_entry && old_entry->hash == hash && 
                     old_entry->key_size == key_size &&
                     memcmp(old_entry->key_data, key, key_size) == 0);
    
    if (table->stats) {
        if (is_update) {
            table->stats->updates++;
        }
    }
    
    /* v2.1: Use preallocated slot */
    smc_artifact_entry_t *entry = (smc_artifact_entry_t *)table->slots[index].data;
    
    if (!is_update && old_entry && table->stats) {
        /* Collision eviction - old entry is overwritten */
        table->stats->evictions++;
    }
    
    entry->hash = hash;
    entry->key_size = key_size;
    entry->value_size = value_size;
    memcpy(entry->key_data, key, key_size);
    void *value_ptr = entry->key_data + key_size;
    if (value && value_size > 0) {
        memcpy(value_ptr, value, value_size);
    }
    table->slots[index].occupied = 1;
    
    table->entries[index] = entry;
    return SMC_OK;
}

int smc_artifact_table_remove(smc_artifact_table_t *table,
                               const void *key, size_t key_size) {
    if (!table || !table->configured) {
        return SMC_ERR_INIT;
    }
    if (!key || key_size == 0) {
        return SMC_ERR_INVALID;
    }
    if (key_size > table->max_key_size) {
        return SMC_ERR_SIZE;
    }
    
    uint32_t hash = smc_byte_hash(key, key_size);
    size_t index = hash % table->entry_count;
    smc_artifact_entry_t *entry = table->entries[index];
    
    if (entry && entry->hash == hash && entry->key_size == key_size &&
        memcmp(entry->key_data, key, key_size) == 0) {
        /* v2.1: For preallocated slots, just mark as unoccupied */
        table->slots[index].occupied = 0;
        table->entries[index] = NULL;
        if (table->stats) {
            table->stats->removes++;
        }
        return SMC_OK;
    }
    
    return SMC_ERR_NOT_FOUND;
}

int smc_artifact_table_clear(smc_artifact_table_t *table) {
    if (!table || !table->configured) {
        return SMC_ERR_INIT;
    }
    
    /* v2.1: Clear all slot occupancy flags */
    for (size_t i = 0; i < table->entry_count; i++) {
        table->slots[i].occupied = 0;
        table->entries[i] = NULL;
    }
    
    if (table->stats) {
        table->stats->clears++;
    }
    
    return SMC_OK;
}