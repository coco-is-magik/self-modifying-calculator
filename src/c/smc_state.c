/* smc_state.c — dirty-state tracking implementation for SMC
 *
 * This file implements a direct-mapped hash table for tracking state
 * changes by opaque binary keys.
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

static size_t smc_state_memory_needed(const smc_state_config_t *config) {
    size_t entries = config->max_entries;
    if (entries == 0) entries = SMC_STATE_DEFAULT_MAX_ENTRIES;
    
    size_t key_size = config->max_key_size;
    if (key_size == 0) key_size = SMC_STATE_DEFAULT_MAX_KEY_SIZE;
    
    size_t state_size = config->max_state_size;
    if (state_size == 0) state_size = SMC_STATE_DEFAULT_MAX_STATE_SIZE;
    
    size_t entry_header = sizeof(smc_state_entry_t);
    return entries * (sizeof(smc_state_entry_t *) + entry_header + key_size + state_size);
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
    
    size_t memory_needed = smc_state_memory_needed(config);
    size_t budget = config->memory_budget_bytes;
    if (budget != 0 && memory_needed > budget) {
        return SMC_ERR_CAPACITY;
    }
    
    table->entries = (smc_state_entry_t **)calloc(max_entries, sizeof(smc_state_entry_t *));
    if (!table->entries) {
        return SMC_ERR_INIT;
    }
    
    table->entry_count = max_entries;
    table->max_key_size = max_key_size;
    table->max_state_size = max_state_size;
    table->memory_budget = budget != 0 ? budget : memory_needed;
    table->configured = 1;
    table->stats = stats;
    
    if (stats) {
        memset(stats, 0, sizeof(*stats));
    }
    
    return SMC_OK;
}

void smc_state_table_destroy(smc_state_table_t *table) {
    if (!table) return;
    
    if (table->entries) {
        for (size_t i = 0; i < table->entry_count; i++) {
            free(table->entries[i]);
        }
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
    if (!key || key_size == 0 || !state || !out_changed) {
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
            memcmp(stored_state, state, state_size) == 0) {
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
        
        /* Reallocate if state size differs */
        size_t entry_size = sizeof(smc_state_entry_t) + key_size + state_size;
        if (entry->state_size != state_size) {
            free(entry);
            entry = (smc_state_entry_t *)malloc(entry_size);
            if (!entry) return SMC_ERR_INIT;
        }
        
        entry->hash = hash;
        entry->key_size = key_size;
        entry->state_size = state_size;
        memcpy(entry->key_data, key, key_size);
        memcpy(entry->key_data + key_size, state, state_size);
        table->entries[index] = entry;
        return SMC_OK;
    }
    
    /* First observation of this key - store state */
    *out_changed = 1;
    if (table->stats) {
        table->stats->changed++;
        table->stats->stores++;
    }
    
    size_t entry_size = sizeof(smc_state_entry_t) + key_size + state_size;
    smc_state_entry_t *new_entry = (smc_state_entry_t *)malloc(entry_size);
    if (!new_entry) {
        return SMC_ERR_INIT;
    }
    
    new_entry->hash = hash;
    new_entry->key_size = key_size;
    new_entry->state_size = state_size;
    memcpy(new_entry->key_data, key, key_size);
    memcpy(new_entry->key_data + key_size, state, state_size);
    
    /* Free old entry on collision */
    if (entry) {
        free(entry); /* Direct-mapped: evict on collision */
    }
    
    table->entries[index] = new_entry;
    return SMC_OK;
}

int smc_state_table_clear(smc_state_table_t *table) {
    if (!table || !table->configured) {
        return SMC_ERR_INIT;
    }
    
    for (size_t i = 0; i < table->entry_count; i++) {
        if (table->entries[i]) {
            free(table->entries[i]);
            table->entries[i] = NULL;
        }
    }
    
    return SMC_OK;
}