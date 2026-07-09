/* smc_artifact.c — artifact cache implementation for SMC
 *
 * This file implements a direct-mapped hash table for caching arbitrary
 * binary artifacts by opaque binary keys.
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

/* Compute required memory for a configuration */
static size_t smc_artifact_memory_needed(const smc_artifact_config_t *config) {
    size_t entries = config->max_entries;
    if (entries == 0) entries = SMC_ARTIFACT_DEFAULT_MAX_ENTRIES;
    
    size_t key_size = config->max_key_size;
    if (key_size == 0) key_size = SMC_ARTIFACT_DEFAULT_MAX_KEY_SIZE;
    
    size_t value_size = config->max_value_size;
    if (value_size == 0) value_size = SMC_ARTIFACT_DEFAULT_MAX_VALUE_SIZE;
    
    /* Entry struct: pointer array + header + key + max value */
    size_t entry_header = sizeof(smc_artifact_entry_t);
    return entries * (sizeof(smc_artifact_entry_t *) + entry_header + key_size + value_size);
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
    
    /* Check memory budget */
    size_t memory_needed = smc_artifact_memory_needed(config);
    size_t budget = config->memory_budget_bytes;
    if (budget != 0 && memory_needed > budget) {
        return SMC_ERR_CAPACITY;
    }
    
    /* Allocate entry pointer array */
    table->entries = (smc_artifact_entry_t **)calloc(max_entries, sizeof(smc_artifact_entry_t *));
    if (!table->entries) {
        return SMC_ERR_INIT;
    }
    
    table->entry_count = max_entries;
    table->max_key_size = max_key_size;
    table->max_value_size = max_value_size;
    table->memory_budget = budget != 0 ? budget : memory_needed;
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
        } else {
            /* New entry or eviction */
        }
    }
    
    /* Allocate entry: header + key + value */
    size_t entry_size = sizeof(smc_artifact_entry_t) + key_size + value_size;
    smc_artifact_entry_t *entry = (smc_artifact_entry_t *)malloc(entry_size);
    if (!entry) {
        return SMC_ERR_INIT;
    }
    
    entry->hash = hash;
    entry->key_size = key_size;
    entry->value_size = value_size;
    memcpy(entry->key_data, key, key_size);
    void *value_ptr = entry->key_data + key_size;
    if (value && value_size > 0) {
        memcpy(value_ptr, value, value_size);
    }
    
    /* Free old entry on collision (eviction) */
    if (!is_update && old_entry) {
        free(old_entry);
        if (table->stats) {
            table->stats->evictions++;
        }
    }
    
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
        free(entry);
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
    
    for (size_t i = 0; i < table->entry_count; i++) {
        if (table->entries[i]) {
            free(table->entries[i]);
            table->entries[i] = NULL;
        }
    }
    
    if (table->stats) {
        table->stats->clears++;
    }
    
    return SMC_OK;
}