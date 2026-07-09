# SMC Artifact Cache — Design and Usage Guide

> **Version**: 2  
> **Scope**: This document describes the artifact cache and dirty-state tracking subsystems in SMC v2.

---

## 1. Overview

SMC v2 introduces a general-purpose binary artifact cache designed for hot-path optimization in C applications. It addresses the pattern that emerged from the ASCII renderer work: skip redundant computation by recognizing repeated state.

Two subsystems are provided:

1. **Artifact Cache** — Cache arbitrary binary data by opaque keys
2. **Dirty-State Tracker** — Detect when state has changed to avoid redundant work

Both use direct-mapped hash tables for predictable performance.

---

## 2. Artifact Cache API

### 2.1 Configuration

```c
smc_artifact_config_t config = {
    .max_entries = 4096,         // Default: 4096
    .max_key_size = 64,          // Default: 64 bytes
    .max_value_size = 4096,       // Default: 4096 bytes
    .memory_budget_bytes = 0      // Auto-computed if 0
};

int rc = smc_artifact_configure(ctx, &config);
```

Configuration must be called once per context before any artifact operations.

**Note (v2.0)**: Entry memory is allocated on each `smc_artifact_store` call using `malloc`. This is acceptable for the MVP but may cause allocation churn in hot paths. A future version (v2.1) will preallocate fixed slots for deterministic performance.

### 2.2 Operations

```c
/* Store an artifact */
int rc = smc_artifact_store(ctx, key, key_size, value, value_size);

/* Lookup an artifact */
int rc = smc_artifact_lookup(ctx, key, key_size, out_buffer, buffer_size, &out_size);

/* Remove a specific artifact */
int rc = smc_artifact_remove(ctx, key, key_size);

/* Clear all artifacts */
int rc = smc_artifact_clear(ctx);
```

### 2.3 Error Codes

| Code | Meaning |
|------|---------|
| `SMC_OK` | Success (for lookup, means hit) |
| `SMC_ERR_NOT_FOUND` | Cache miss |
| `SMC_ERR_SIZE` | Key or value exceeds configured size |
| `SMC_ERR_CAPACITY` | Configuration exceeds memory budget |
| `SMC_ERR_INVALID` | Null pointer or double-configure |
| `SMC_ERR_INIT` | Not configured or context invalid |

### 2.4 Statistics

```c
smc_artifact_stats_t stats;
smc_artifact_get_stats(ctx, &stats);

// Fields:
//   lookups, hits, misses
//   stores, updates, evictions, removes, clears
//   bytes_stored, bytes_returned
```

---

## 3. Dirty-State Tracking API

### 3.1 Configuration

```c
smc_state_config_t config = {0}; // Use defaults
int rc = smc_state_configure(ctx, &config);
```

Independent from artifact cache configuration.

### 3.2 Operations

```c
/* Check if state changed */
int changed = 0;
int rc = smc_state_changed(ctx, key, key_size, state, state_size, &changed);

/* Clear all tracked state */
int rc = smc_state_clear(ctx);
```

### 3.3 Behavior

- **First observation of a key**: Returns `changed=1`, stores the state
- **Same key, same state**: Returns `changed=0` (no work needed)
- **Same key, different state**: Returns `changed=1`, updates stored state

This allows the renderer pattern: check once per frame, skip rasterization on unchanged cells.

---

## 4. Renderer Pattern Example

```c
/* Cell with glyph, colors, etc. */
typedef struct {
    uint32_t id;
    uint32_t glyph_id;
    uint32_t fg_rgba;
    uint32_t bg_rgba;
} CellState;

/* Glyph block (rasterized output) */
typedef struct {
    uint32_t pixels[64];  // 256 bytes
} GlyphBlock;

/* In the render loop */
for (each_cell) {
    int changed = 0;
    
    /* Skip unchanged cells */
    if (smc_state_changed(ctx, &cell->id, sizeof(cell->id),
                          &cell, sizeof(CellState), &changed) != SMC_OK || !changed) {
        continue;
    }
    
    /* Check artifact cache */
    struct { uint32_t glyph_id, fg, bg; } key = {cell->glyph_id, cell->fg_rgba, cell->bg_rgba};
    GlyphBlock block;
    
    if (smc_artifact_lookup(ctx, &key, sizeof(key),
                           &block, sizeof(block), NULL) == SMC_OK) {
        blit_block(&block); /* Cache hit */
    } else {
        rasterize_glyph(cell, &block); /* Expensive work */
        smc_artifact_store(ctx, &key, sizeof(key), &block, sizeof(block));
        blit_block(&block);
    }
}
```

---

## 5. Implementation Notes

### 5.1 Hash Function

Both caches use FNV-1a 32-bit hash for byte sequences. The hash is computed at call time and used for direct-mapped indexing.

### 5.2 Direct-Mapped Eviction

```
index = hash(key) % max_entries
```

If a slot contains a different key, the entry is evicted. The `evictions` stat counter tracks this.

### 5.3 Memory Layout

Entries store key and value contiguously:
```
[header: hash, key_size, value_size][key bytes][value bytes]
```

This avoids separate allocations and improves cache locality.

### 5.4 Thread Safety

For v1, contexts are assumed to be externally synchronized. Use one context per thread or provide your own locking.

---

## 6. Feature Detection

```c
uint32_t features = smc_features();
if (features & SMC_FEATURE_ARTIFACT_CACHE) {
    // Artifact cache available
}
if (features & SMC_FEATURE_STATE_TRACKING) {
    // Dirty-state tracking available
}