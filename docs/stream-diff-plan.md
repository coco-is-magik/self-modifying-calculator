# Generalized Indexed Stream / SoA Diff API Implementation Plan

## Version

Draft v1.0 — approved for implementation.

## Context

SMC has a validated dense indexed batch state API (`smc_state_diff_indexed_batch`).
Renderer integration proved the API is viable, but the remaining gap is dominated by
temporary packed state construction, not the SMC diff itself. The next direction is
to avoid this temporary packing by allowing SMC to compare multiple existing fields
or streams directly.

## Objective

Plan a generalized indexed stream / struct-of-arrays diff API. Callers describe one
logical state record as a collection of byte streams. For each record index `i`, SMC
compares the bytes from each stream at index `i` against previous stored state, and
marks index `i` dirty if any stream field changed.

This must remain general. No renderer-specific API.

## Recommended Public API

```c
typedef struct {
    const void *data;     /* Pointer to first record; NULL if record_count == 0 or field_size == 0 */
    size_t stride;        /* Byte distance between consecutive records; must be >= field_size */
    size_t field_size;    /* Size of each field in bytes (0 for empty stream) */
} smc_state_stream_t;

SMC_API int smc_state_diff_indexed_streams(
    smc_context_t *ctx,
    const smc_state_stream_t *streams,
    size_t stream_count,
    size_t record_count,
    uint32_t *dirty_indices,
    size_t dirty_capacity,
    size_t *out_dirty_count
);
```

## State Storage Model

**Recommendation: Option A — packed contiguous per-record snapshots.**

Reuse the existing `smc_indexed_state_table_t` structure unchanged. Internal storage
layout is:

```
[record0 stream0][record0 stream1]...[record0 streamN]
[record1 stream0][record1 stream1]...[record1 streamN]
...
```

### Evaluation of Options

| Option | Memory | Compare Cost | Copy/Update Cost | Cache Locality | Simplicity | C99 Portability | Stats Fit |
|--------|--------|--------------|------------------|----------------|------------|-----------------|-----------|
| A: packed contiguous | Optimal | Fast (per-field memcmp) | Fast (all fields memcpy if dirty) | Excellent | Simple | Full | Perfect |
| B: separate per-stream | Higher overhead | Slower (scattered) | Slower (multiple copies) | Poor | Complex | Full | Poor |
| C: hash/fingerprint | Higher | Slower (hash compute) | Slower | Poor | Complex | Full | Poor |

Packed contiguous is the clear winner for this API.

## API Semantics

- Record `i` maps to indexed slot `i`.
- Each stream contributes `field_size` bytes to the logical state.
- `stream.data` may be NULL when `record_count == 0` or `field_size == 0`.
- `stream.stride` must be nonzero and `>= field_size` when `field_size > 0`.
- `stream_count` must be `> 0` unless `record_count == 0`.
- `dirty_indices` may be NULL only when `dirty_capacity == 0`.
- `dirty_capacity` overflow writes partial indices but reports full dirty count.
- State is updated for all changed records even when dirty index output is truncated.
- `out_dirty_count` must be non-NULL.
- `record_count` must not exceed configured indexed capacity.
- Zero-size streams contribute nothing to logical state; `total_field_size == 0` only
  valid when configured `state_size == 0`.
- Duplicate/overlapping streams are not recommended; behavior is implementation-defined.

## Configuration Model

Stream diffing reuses the existing indexed state configuration:

```c
smc_state_indexed_configure(ctx, &config);
```

Compatibility rule:

```
sum(streams[i].field_size for all i) == config.state_size
```

If this is violated, return `SMC_ERR_SIZE`. This keeps the API simple: one
configuration, multiple usage patterns.

## Layout Flexibility

### Struct-of-arrays

```c
uint8_t glyphs[41600];
uint8_t fg[41600];
smc_state_stream_t streams[] = {
    {glyphs, 1, 1},
    {fg,     1, 1},
};
```

### Array-of-structs

```c
typedef struct { uint8_t glyph, fg, bg; } Cell;
Cell cells[41600];
smc_state_stream_t streams[] = {
    {&cells[0].glyph, sizeof(Cell), 1},
    {&cells[0].fg,    sizeof(Cell), 1},
    {&cells[0].bg,    sizeof(Cell), 1},
};
```

The `stride` parameter handles both naturally.

## Performance Strategy

- Compare all stream fields directly from source arrays.
- Internal packed previous-state snapshots reused.
- For changed records, copy ALL fields into the stored snapshot (no partial updates).
- No short-circuit comparison in the first implementation.
- Fixed-size stream field kernels for 1/2/4/8 byte fields can be added later.
- Stats overhead kept minimal.

## Stats Model

Reuse existing `smc_state_indexed_stats_t` without extension:

| Stat | Meaning for stream diff |
|------|------------------------|
| `checks` | Records processed |
| `changed` | Dirty records |
| `unchanged` | Clean records |
| `stores` | Records whose snapshot was updated |
| `bytes_compared` | Total field bytes compared |

## Validation Rules

```c
if (record_count == 0) {
    *out_dirty_count = 0;
    return SMC_OK;
}
if (stream_count == 0) return SMC_ERR_INVALID;
if (streams == NULL) return SMC_ERR_INVALID;
if (out_dirty_count == NULL) return SMC_ERR_INVALID;
if (dirty_indices == NULL && dirty_capacity > 0) return SMC_ERR_INVALID;
if (record_count > table->count) return SMC_ERR_SIZE;

total_field_size = 0;
for each stream:
    if field_size == 0: continue
    if stride == 0: return SMC_ERR_INVALID
    if stride < field_size: return SMC_ERR_SIZE
    total_field_size += field_size

if (total_field_size != table->state_size) return SMC_ERR_SIZE;
```

## Compatibility

The new API must not break:
- `smc_state_changed()`
- `smc_state_changed_index()`
- `smc_state_diff_indexed_batch()`
- Artifact cache APIs
- Python bindings
- Existing tests

## Testing Plan

Add to `tests/c/test_indexed_state.c`:

1. First call all changed
2. Second identical call all unchanged
3. One changed stream field marks one dirty record
4. Multiple stream fields changed
5. Multiple records changed
6. Dirty indices returned in ascending order
7. Capacity overflow updates all stored state
8. NULL streams validation
9. NULL stream data validation
10. Zero-size stream behavior
11. Stride larger than field_size
12. Stride smaller than field_size rejected
13. record_count too large rejected
14. Mixed-size streams: 1+3+3 bytes
15. Mixed-size streams: 1+2+4 bytes
16. Mixed-size streams: seven 1-byte streams
17. AoS layout test
18. SoA layout test
19. Overlapping streams behavior
20. Stats correctness
21. Clear/reset behavior
22. Total stream state size mismatch: config state_size=8, streams sum=7 -> SMC_ERR_SIZE
23. Zero-total-size streams: config state_size=0, streams all field_size=0, frame 1 dirty, frame 2 unchanged
24. Stale-field prevention test (critical correctness test)

## Stale-Field Prevention Test

```c
/* Configure for 1+3+3 = 7 byte state */
smc_state_indexed_config_t config = { .count = 100, .state_size = 7 };
smc_state_indexed_configure(ctx, &config);

uint8_t field_a[100];      /* 1 byte per record */
uint8_t field_b[100 * 3];  /* 3 bytes per record */
uint8_t field_c[100 * 3];  /* 3 bytes per record */

smc_state_stream_t streams[3] = {
    {field_a, 1, 1},
    {field_b, 3, 3},
    {field_c, 3, 3},
};

/* Frame 1: all 1 */
/* Frame 2: change field_a and field_b in record 42 -> expect 1 dirty */
/* Frame 3: same as frame 2 -> expect 0 dirty */
```

If the implementation has a stale-field bug, frame 3 would incorrectly report
record 42 as changed.

## Benchmark Plan

Extend `tests/benchmarks/benchmark_indexed_state.c` to compare:

1. Current packed uint64_t batch API **including packing loop cost**
2. New stream/SoA diff API
3. Custom direct comparison loop

Workload:
- 41,600 records
- Change rates: 0%, 1%, 10%, 50%, 100%
- Field layouts:
  - 7 one-byte streams
  - 1 uint64_t packed stream
  - Mixed: 1-byte glyph + 3-byte fg + 3-byte bg
- AoS and SoA variants

Output columns:

```
records | total_bytes | layout_type | change_rate | packed_ns/op | stream_ns/op | direct_ns/op | packed_ms | stream_ms | direct_ms
```

Example row:

```
41600   | 7           | mixed_1_3_3 | 1%          | 11.8         | 5.1          | 4.6          | 0.491     | 0.212     | 0.191
```

Packing expression (with explicit casts):

```c
packed_states[i] =
    ((uint64_t)glyph[i] << 0)  |
    ((uint64_t)fg_r[i]  << 8)  |
    ((uint64_t)fg_g[i]  << 16) |
    ((uint64_t)fg_b[i]  << 24) |
    ((uint64_t)bg_r[i]  << 32) |
    ((uint64_t)bg_g[i]  << 40) |
    ((uint64_t)bg_b[i]  << 48);
```

## Documentation Plan

Update `docs/artifact-cache.md` or create `docs/stream-diff.md` explaining:
- When to use indexed batch vs indexed stream diff
- How stream descriptors work
- AoS examples
- SoA examples
- How to avoid padding
- When packed uint64_t is still better
- When stream diffing helps
- When stream diffing may be slower
- Memory budget behavior

Also update `docs/HANDOFF.md` and `docs/CHANGELOG.md` as implementation progresses.

## File-by-File Implementation Plan

### include/smc.h
- Current role: stable ABI public header
- Changes: add `smc_state_stream_t` struct and `smc_state_diff_indexed_streams()` declaration
- Risk: Low (additive API)

### src/c/smc_state.h
- Current role: internal state tracking header
- Changes: add `smc_indexed_state_table_diff_streams()` declaration
- Risk: Low

### src/c/smc_state.c
- Current role: state tracking implementation
- Changes: implement stream diff algorithm using existing packed storage
- Risk: Medium (new core logic, isolated)

### src/c/smc_runtime_stub.c
- Current role: public API dispatch
- Changes: add `smc_state_diff_indexed_streams()` wrapper with validation
- Risk: Low

### python/smc/__init__.py
- Current role: ctypes bindings
- Changes: add `_smc_state_stream_t` struct and `state_diff_indexed_streams()` binding
- Risk: Low

### tests/c/test_indexed_state.c
- Current role: indexed state acceptance tests
- Changes: add 20+ new stream diff tests
- Risk: Low (additive tests)

### tests/benchmarks/benchmark_indexed_state.c
- Current role: indexed state benchmark
- Changes: add stream diff comparison mode with packing cost
- Risk: Low

### CMakeLists.txt
- Current role: build configuration
- Changes: none required (new tests link against existing targets)
- Risk: None

### docs/artifact-cache.md / docs/stream-diff.md
- Current role: documentation
- Changes: add stream diff section or new document
- Risk: Low

### docs/HANDOFF.md
- Current role: project handoff notes
- Changes: summarize stream diff feature and decisions
- Risk: Low

### docs/CHANGELOG.md
- Current role: version changelog
- Changes: add entry for new stream diff API
- Risk: Low

## Evidence Questions

### What would prove the stream API is correct?

- All new stream tests pass, especially stale-field prevention and mixed-size tests.
- Stats counters match expected values for each test.
- Stream diff produces identical dirty indices to an equivalent packed batch diff.

### What would prove it is faster than temporary packed-state construction?

- Benchmark shows `stream_ms` < `packed_ms` for typical renderer workloads.
- Direct loop provides a performance floor; stream diff should be close.

### What would prove it has preserved SMC generality?

- No renderer-specific types in API.
- Mixed field sizes and arbitrary layouts work.
- Both AoS and SoA supported.
- Python bindings work unchanged aside from new function.

### What benchmark result would justify returning to the renderer?

If for the renderer-shaped workload (41,600 records, low change rate, 7 one-byte
streams) the stream API total time is measurably less than the packed batch total
including packing cost, the renderer can adopt it.

## Migration Path from Packed Batch API

Users currently packing into `uint64_t` arrays can migrate incrementally:

1. Configure `state_size` to equal the sum of desired field sizes.
2. Replace the packing loop with `smc_state_stream_t` descriptors pointing at source arrays.
3. Call `smc_state_diff_indexed_streams()` instead of `smc_state_diff_indexed_batch()`.
4. The packed batch API remains available and unchanged.

## Constraints

- Do not remove the packed indexed batch API.
- Do not remove fixed-size batch kernels.
- Do not add renderer-specific concepts.
- Do not add callbacks, persistence, threading, SIMD, intrinsics, or platform-specific assembly.
- Do not use hashing for this dense indexed API.
- C99 strict conformance; use `memcpy` for unaligned safety.
