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

**Note (v2.1)**: All memory is allocated at configuration time. Each entry uses a preallocated slot from a single contiguous buffer. No heap allocation occurs during `smc_artifact_store` or `smc_artifact_lookup` operations.

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

**Note (v2.1)**: All memory is allocated at configuration time. Each state entry uses a preallocated slot from a single contiguous buffer. No heap allocation occurs during `smc_state_changed` operations.

### 3.4 State Statistics

```c
smc_state_stats_t stats;
smc_state_get_stats(ctx, &stats);

// Fields:
//   checks, changed, unchanged
//   stores, evictions
//   bytes_compared
```

The `evictions` counter tracks hash collisions that caused an existing entry to be overwritten.

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
if (features & SMC_FEATURE_INDEXED_STATE_TRACKING) {
    // Indexed-state tracking available (v2.1)
}
```

---

## 7. Performance Considerations

### 7.1 Hot-Path Allocation

The artifact and state caches allocate a single contiguous buffer during configuration. After `smc_artifact_configure()` or `smc_state_configure()` returns, no further heap allocation occurs during normal operations. This makes both APIs suitable for frame-budgeted hot paths in renderers and game engines.

### 7.2 Cache Sizing

For best performance:
- Choose `max_key_size` and `max_value_size` to match your actual data
- Use `memory_budget_bytes` to limit total cache memory
- Expect `evictions` under heavy load; tune `max_entries` to reduce collision rate

### 7.3 Measuring Effectiveness

Users must measure hit rate and operation cost in their own applications:
- Check `smc_artifact_stats_t.hits` vs `lookups`
- Check `smc_state_stats_t.changed` vs `checks`
- SMC does not automatically speed up renderers; it provides the primitives for users to build their own optimization layers.

---

## 8. Indexed-State Tracking for Dense Arrays

For dense array data where each element has a stable integer index (renderers, ECS, tile maps, particle systems), use the indexed state API. It avoids hashing and key comparison overhead, achieving significantly better performance than the generic API.

### 8.1 When to Use

- **Generic `smc_state_changed()`**: Sparse data with arbitrary binary keys
- **Indexed `smc_state_changed_index()`**: Dense arrays with stable integer indices (0 to count-1)
- **Batch `smc_state_diff_indexed_batch()`**: Many dense records checked every frame/tick

### 8.2 Configuration

```c
smc_state_indexed_config_t config = {
    .count = 41600,          // Number of indexed slots
    .state_size = 8,          // Size of each state record
    .memory_budget_bytes = 0   // Auto-computed if 0
};
smc_state_indexed_configure(ctx, &config);
```

### 8.3 Scalar Operations

```c
int changed = 0;
smc_state_changed_index(ctx, index, &state, sizeof(state), &changed);
```

### 8.4 Batch Operations

```c
size_t dirty_count = 0;
smc_state_diff_indexed_batch(ctx, states, count, stride,
                              dirty_indices, capacity, &dirty_count);
```

If `dirty_indices` is NULL and capacity is 0, the function still updates state and counts changes.

### 8.5 Renderer Lesson

For dense grids (e.g., terminal-style renderers with 260×160 cells), the hash-based generic state tracking can be too expensive. The indexed/batch APIs avoid hashing and per-cell key comparison, providing a fast path comparable to hand-written dirty tracking.

Typical benchmark results for 41,600 records with 8-byte state structs:

| Operation | Generic ns/op | Indexed ns/op | Batch ns/op |
|-----------|---------------|---------------|-------------|
| Unchanged | ~250-320      | ~30           | ~15-40      |
| Changed   | ~600-2100     | ~40           | ~15-60      |

The indexed unchanged path is typically 6-10x faster than the generic path. The batch path provides additional speedup by reducing per-call overhead.

## 9. Indexed Stream Diff (v2.2)

For callers whose source data is already split into separate arrays or fields,
the stream diff API avoids the cost of building a temporary packed state array.

### 9.1 When to Use Stream Diff

Use `smc_state_diff_indexed_streams()` when:
- Your state is already stored as separate arrays (SoA) or non-contiguous struct fields (AoS)
- Building a temporary `uint64_t` packed array is measurable overhead
- Field sizes are mixed or not a power of two

Continue using `smc_state_diff_indexed_batch()` when:
- You already have a packed contiguous array
- State fits naturally in 1, 2, 4, or 8 bytes (fixed-size kernels apply)
- Packing cost is negligible or already amortized

### 9.2 Stream Descriptor

```c
typedef struct {
    const void *data;
    size_t stride;
    size_t field_size;
} smc_state_stream_t;
```

- `data` — pointer to the first record of this field
- `stride` — byte distance between consecutive records
- `field_size` — number of bytes contributed by this field

The sum of all `field_size` values must equal the configured `state_size`.

### 9.3 Example: Struct-of-Arrays

```c
uint8_t glyphs[41600];
uint8_t fg[41600 * 3];
uint8_t bg[41600 * 3];

smc_state_stream_t streams[3] = {
    {glyphs, 1, 1},
    {fg,     3, 3},
    {bg,     3, 3},
};

size_t dirty_count = 0;
smc_state_diff_indexed_streams(ctx, streams, 3, 41600,
                                dirty_indices, capacity, &dirty_count);
```

### 9.4 Example: Array-of-Structs

```c
typedef struct {
    uint8_t glyph;
    uint8_t fg[3];
    uint8_t bg[3];
} Cell;

Cell cells[41600];

smc_state_stream_t streams[3] = {
    {&cells[0].glyph, sizeof(Cell), 1},
    {&cells[0].fg[0], sizeof(Cell), 3},
    {&cells[0].bg[0], sizeof(Cell), 3},
};
```

### 9.5 Semantics

- Record `i` maps to indexed slot `i`
- A record is dirty if any field differs from the stored snapshot
- When a record is dirty, ALL fields are copied into the stored snapshot
- Dirty indices are returned in ascending order
- `dirty_capacity` overflow writes partial indices but reports the full count

## 10. Optimized Fixed-Size Batch Kernels (v2.2)

The batch API (`smc_state_diff_indexed_batch`) includes optimized internal kernels for common small state sizes: 1, 2, 4, and 8 bytes. These kernels use `memcpy`-based loads and stores so they remain safe for unaligned input and strictly conforming C99. They are treated as performance candidates; any size that does not beat the generic `memcmp`/`memcpy` baseline by at least 5% falls back to the generic path.

### 10.1 When Kernels Help

Power-of-two sizes (1, 2, 4, and 8 bytes) benefit because the compiler can reduce the comparison to a single integer equality check. The 16-byte candidate was evaluated but did not meet the 5% improvement threshold reliably, so it uses the generic fallback. Non-power-of-two sizes such as 7 or 12 bytes also use the generic byte-by-byte fallback.

### 10.2 Recommended State Representation

Pass compact, deterministic state values such as `uint64_t` arrays. Avoid padded structs with uninitialized padding bytes, because padding can cause false-positive change detection.

Example transformation:

```c
/* AVOID: struct with implicit padding */
typedef struct {
    uint16_t glyph_id;   /* 2 bytes */
    uint16_t flags;      /* 2 bytes */
    uint32_t color;      /* 4 bytes */
} CellState;             /* 8 bytes total, but padding is possible */

CellState states[41600];
/* If any padding byte is not explicitly zeroed, smc_state_diff_indexed_batch
 * may report spurious changes. */

/* GOOD: explicit uint64_t packing */
uint64_t states[41600];
for (size_t i = 0; i < 41600; i++) {
    uint16_t glyph_id = ...;
    uint16_t flags    = ...;
    uint32_t color    = ...;
    states[i] = ((uint64_t)glyph_id << 0)  |
                ((uint64_t)flags    << 16) |
                ((uint64_t)color    << 32);
}
```

This representation:
- Enables the 8-byte fixed-size kernel.
- Eliminates padding-related false positives.
- Keeps the comparison as a single integer operation.

### 10.3 Measuring Kernel Performance

Build both the fixed-kernel and generic-only benchmark targets:

```bash
cmake -B build -S . -DSMC_GENERATED_SOURCE=/path/to/build/smc_generated.c
cmake --build build --target benchmark_indexed_state benchmark_indexed_baseline
./build/benchmark_indexed_state
./build/benchmark_indexed_baseline
```

Both executables run the same public API benchmark. The baseline executable links against a library built with `SMC_DISABLE_FIXED_BATCH_KERNELS`, so it always uses the generic `memcmp`/`memcpy` path. Each executable reports median ns/op across 3 internal passes. Compare the two outputs to measure the improvement of the fixed-size kernels against the real public API baseline.

### 10.4 Generality Guarantee

The public API is unchanged. Arbitrary state sizes continue to use the generic `memcmp`/`memcpy` fallback. The fixed-size kernels are an internal optimization and do not affect portability, alignment safety, or API semantics.
