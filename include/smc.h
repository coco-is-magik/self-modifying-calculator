/* smc.h — stable ABI v1 for the Self-Modifying Calculator
 *
 * ABI CONTRACT (v1)
 * =================
 * This header defines the stable application-binary interface for SMC.  The
 * goal is to let host programs (C, C++, Python via ctypes, etc.) evaluate
 * mathematical expressions and, in production builds, call pre-generated hot
 * paths by stable integer ID.
 *
 * Versioning
 * ------------
 *   SMC_ABI_VERSION is a compile-time constant.  It is bumped only when a
 *   future release changes the layout of public structs, the calling convention,
 *   or the semantics of public functions.  The runtime exposes its ABI version
 *   via smc_abi_version().  Generated artifacts embed their own ABI version;
 *   smc_init() rejects a generated table whose ABI version does not match the
 *   runtime's ABI version.
 *
 * Struct layout
 * -------------
 *   smc_error_t is public and frozen: a 32-bit code followed by a 256-byte
 *   NUL-terminated message buffer.  Host code may read these fields directly.
 *
 *   smc_context_t is opaque.  Its layout is private and may change without
 *   bumping the ABI version as long as the public API remains source- and
 *   binary-compatible.
 *
 * Calling convention
 * ------------------
 *   All public functions use C linkage and cdecl calling convention.  They
 *   return an int status code: 0 (SMC_OK) on success, a negative error code
 *   on failure.  Output values are written through pointer arguments.
 *
 * Ownership & lifetimes
 * -----------------------
 *   - smc_context_create returns a heap-allocated context.  It must be
 *     destroyed with smc_context_destroy.  No other public function returns
 *     heap memory.
 *   - smc_expr_source returns a pointer to static storage inside the generated
 *     dispatch table.  The pointer is valid for the lifetime of the process (or
 *     until smc_shutdown, if the generated table is tied to library lifetime).
 *   - smc_last_error and smc_last_error_with return pointers to thread-local
 *     or global static storage.  The pointer is valid until the next call
 *     that modifies the same error slot.
 *
 * Null-pointer behavior
 * ---------------------
 *   Functions that accept pointers return SMC_ERR_INVALID if a required
 *   pointer is NULL, unless otherwise documented.  Functions that accept
 *   optional pointers (e.g., context pointers for the global-context variants)
 *   document their behavior explicitly.
 *
 * Pre-initialization rules
 * ------------------------
 *   Before smc_init() returns successfully, the only safe functions are
 *   smc_init() itself and smc_error_string().  All other functions return
 *   SMC_ERR_INIT if called before the library is initialized.
 *
 * Thread safety
 * -------------
 *   - smc_call_* functions are stateless and read-only.  They are safe to
 *     call concurrently from multiple threads, provided the generated dispatch
 *     table is immutable.
 *   - The implicit global context (smc_eval_*, smc_set_variable_double,
 *     smc_cache_*, etc.) is single-threaded by default.  Build with
 *     SMC_THREAD_SAFE=ON to enable internal locking, or use per-thread
 *     smc_context_t* instances.
 *   - smc_init and smc_shutdown are not thread-safe and must be called once
 *     per process, from a single thread.
 *
 * Tiers
 * -----
 *   Tier 1 (smc_eval_*): expression-string evaluation.  Intended for
 *   tooling, prototyping, and development.  Not recommended for frame-budgeted
 *   hot loops because it parses strings on every call.
 *
 *   Tier 2 (smc_call_*): generated-code dispatch by stable expression ID.
 *   This is the production hot path: no string parsing, no heap allocation,
 *   deterministic performance.
 */

#ifndef SMC_H
#define SMC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* Symbol visibility                                                          */
/* -------------------------------------------------------------------------- */

#if defined(_WIN32) || defined(__CYGWIN__)
#  if defined(SMC_BUILD_SHARED)
#    define SMC_API __declspec(dllexport)
#  elif defined(SMC_USE_SHARED)
#    define SMC_API __declspec(dllimport)
#  else
#    define SMC_API
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  define SMC_API __attribute__((visibility("default")))
#else
#  define SMC_API
#endif

/* -------------------------------------------------------------------------- */
/* ABI version                                                                */
/* -------------------------------------------------------------------------- */

#define SMC_ABI_VERSION 1

/* -------------------------------------------------------------------------- */
/* Error codes                                                                */
/* -------------------------------------------------------------------------- */

#define SMC_OK             0
#define SMC_ERR_INIT      -1   /* library not initialized or init failed */
#define SMC_ERR_PARSE     -2   /* expression could not be parsed */
#define SMC_ERR_EVAL      -3   /* expression evaluated to an error */
#define SMC_ERR_NOT_IMPL  -4   /* feature not implemented in this runtime */
#define SMC_ERR_IO        -5   /* file or I/O error */
#define SMC_ERR_INVALID   -6   /* invalid argument (e.g., null pointer) */
#define SMC_ERR_ABI       -7   /* ABI version mismatch (runtime vs. generated) */
#define SMC_ERR_ARITY     -8   /* wrong number of arguments for expression ID */
#define SMC_ERR_NOT_FOUND -9   /* expression ID not present in generated table */
#define SMC_ERR_THREAD    -10  /* thread-safety violation */
#define SMC_ERR_SHUTDOWN  -11  /* library has been shut down */

/* -------------------------------------------------------------------------- */
/* Public types                                                               */
/* -------------------------------------------------------------------------- */

/* Opaque context handle.  Layout is private. */
typedef struct smc_context smc_context_t;

/* Stable expression identifier.  IDs are assigned deterministically by the
 * build-time generator and are valid for the lifetime of the generated table. */
typedef uint32_t smc_expr_id_t;

/* Public error object.  Layout is frozen across ABI v1. */
struct smc_error {
    int   code;            /* SMC error code */
    char  message[256];    /* human-readable message, NUL-terminated */
};
typedef struct smc_error smc_error_t;

/* Observability counters.  Layout is frozen across ABI v1.
 * All counters are monotonically increasing unsigned values. */
struct smc_stats {
    uint64_t total_calls;      /* total smc_call_* invocations */
    uint64_t generated_hits;   /* calls that resolved to a generated expression */
    uint64_t fallback_evals;   /* calls that fell back to Tier 1 evaluation */
    uint64_t invalid_ids;      /* calls with an unknown expression ID */
    uint64_t arity_errors;     /* calls with a wrong argument count */
    uint64_t invalid_calls;    /* calls with invalid arguments (e.g. null out) */
    uint64_t parse_errors;     /* Tier 1 parse failures */
    int      last_error_code;  /* code of the most recently recorded error */
};
typedef struct smc_stats smc_stats_t;

/* -------------------------------------------------------------------------- */
/* Introspection                                                              */
/* -------------------------------------------------------------------------- */

/* Return the compile-time ABI version of the runtime library.  Safe to call
 * before smc_init(). */
SMC_API int smc_abi_version(void);

/* Return a short string identifying the runtime kind:
 *   "stub"    - standalone C runtime (no SBCL dependency)
 *   "sbcl"    - SBCL-backed runtime (placeholder in v1)
 *   "unknown" - unrecognized runtime
 * Safe to call before smc_init(). */
SMC_API const char *smc_runtime_kind(void);

/* -------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

/* One-time library initialization. Must be called before any other API except
 * smc_abi_version(), smc_runtime_kind(), and smc_error_string().
 *
 * Thread safety: not thread-safe.  Call once per process from a single
 * thread before any other SMC API.
 *
 * Returns SMC_OK on success, SMC_ERR_INIT on failure, or SMC_ERR_ABI if the
 * linked generated dispatch table is incompatible with this runtime. */
SMC_API int smc_init(void);

/* One-time library shutdown. Releases global resources.
 *
 * Thread safety: not thread-safe.  After this returns, only smc_abi_version(),
 * smc_runtime_kind(), and smc_error_string() remain safe to call. */
SMC_API int smc_shutdown(void);

/* Create an isolated context with the given optimization level (1..3).
 *
 * Ownership: the caller owns the returned context and must destroy it with
 * smc_context_destroy().
 *
 * Thread safety: the returned context is not thread-safe unless external
 * synchronization is provided.  Each thread should use its own context. */
SMC_API smc_context_t *smc_context_create(int level);

/* Destroy a context created with smc_context_create().  Passing NULL is a no-op. */
SMC_API void smc_context_destroy(smc_context_t *ctx);

/* -------------------------------------------------------------------------- */
/* Global context convenience API                                             */
/* -------------------------------------------------------------------------- */

/* Set the optimization level of the implicit global context.
 *
 * Thread safety: the global context is single-threaded by default. */
SMC_API int smc_set_optimization_level(int level);

/* Get the optimization level of the implicit global context.
 * Returns 1 if the library is not initialized. */
SMC_API int smc_get_optimization_level(void);

/* -------------------------------------------------------------------------- */
/* Tier 1: Development / tooling / embedded SBCL — expression strings       */
/* -------------------------------------------------------------------------- */

/* @tooling Evaluate a mathematical expression string and write the result to
 * *out.  This function parses EXPR on every call and is intended for
 * development, not production hot loops.
 *
 * Preconditions: smc_init() has succeeded; EXPR and OUT are non-NULL.
 * Thread safety: uses the single-threaded global context by default. */
SMC_API int smc_eval_double(const char *expr, double *out);
SMC_API int smc_eval_double_with(smc_context_t *ctx, const char *expr, double *out);

SMC_API int smc_eval_float(const char *expr, float *out);
SMC_API int smc_eval_float_with(smc_context_t *ctx, const char *expr, float *out);

SMC_API int smc_eval_int(const char *expr, int64_t *out);
SMC_API int smc_eval_int_with(smc_context_t *ctx, const char *expr, int64_t *out);

/* -------------------------------------------------------------------------- */
/* Tier 2: Production generated-code — expression IDs, no strings             */
/* -------------------------------------------------------------------------- */

/* @production Call a cached expression by stable ID, passing only its free
 * variables in ARGS.  This is the hot path: no parsing, no allocation, and
 * (in the default build) no locking.
 *
 * Preconditions: smc_init() has succeeded; OUT is non-NULL; ARGS is non-NULL
 * if ARGC > 0.
 * Thread safety: safe to call concurrently from multiple threads.
 * Returns SMC_OK on success, SMC_ERR_NOT_FOUND for an unknown ID, or
 * SMC_ERR_ARITY if ARGC does not match the expression's arity. */
SMC_API int smc_call_double(smc_expr_id_t expr_id,
                            const double *args, size_t argc,
                            double *out);

SMC_API int smc_call_float(smc_expr_id_t expr_id,
                           const float *args, size_t argc,
                           float *out);

SMC_API int smc_call_int(smc_expr_id_t expr_id,
                         const int64_t *args, size_t argc,
                         int64_t *out);

/* -------------------------------------------------------------------------- */
/* Variables                                                                  */
/* -------------------------------------------------------------------------- */

/* Bind a variable in the global context before evaluation.  The binding is used
 * by subsequent Tier 1 expression evaluations.
 *
 * Thread safety: uses the single-threaded global context by default. */
SMC_API int smc_set_variable_double(const char *name, double value);
SMC_API int smc_set_variable_double_with(smc_context_t *ctx, const char *name, double value);

/* Clear all variable bindings in the global context. */
SMC_API int smc_clear_variables(void);
SMC_API int smc_clear_variables_with(smc_context_t *ctx);

/* -------------------------------------------------------------------------- */
/* Cache control                                                              */
/* -------------------------------------------------------------------------- */

/* Clear the cache.  In the stub runtime this is a no-op that returns SMC_OK. */
SMC_API int smc_cache_clear(void);
SMC_API int smc_cache_clear_with(smc_context_t *ctx);

/* Save/load the cache.  These require an SBCL-backed runtime or a generated
 * build and return SMC_ERR_NOT_IMPL in the standalone stub runtime. */
SMC_API int smc_cache_save(const char *path);
SMC_API int smc_cache_save_with(smc_context_t *ctx, const char *path);

SMC_API int smc_cache_load(const char *path);
SMC_API int smc_cache_load_with(smc_context_t *ctx, const char *path);

/* -------------------------------------------------------------------------- */
/* Source generation (build-time optimizer output)                            */
/* -------------------------------------------------------------------------- */

/* Generate a C source file containing hot cached expressions.  This requires
 * an SBCL-backed runtime and returns SMC_ERR_NOT_IMPL in the stub runtime. */
SMC_API int smc_generate_c_source(const char *out_path);
SMC_API int smc_generate_c_source_with(smc_context_t *ctx, const char *out_path);

/* -------------------------------------------------------------------------- */
/* Expression metadata (Tier 2)                                                 */
/* -------------------------------------------------------------------------- */

/* Return the number of expressions available in the generated dispatch table.
 * Returns 0 if no generated table is linked. */
SMC_API int smc_expr_count(void);

/* Return the arity (number of free variables) of expression ID.
 * Returns 1 if the ID is not present. */
SMC_API size_t smc_expr_arity(smc_expr_id_t id);

/* Return the original expression string for expression ID, or NULL if the ID
 * is not present.  The returned pointer points to static storage in the
 * generated dispatch table and is valid for the lifetime of the process. */
SMC_API const char *smc_expr_source(smc_expr_id_t id);

/* -------------------------------------------------------------------------- */
/* Error handling                                                             */
/* -------------------------------------------------------------------------- */

/* Return a human-readable string for an error code.  Safe to call before
 * smc_init(). */
SMC_API const char *smc_error_string(int code);

/* Return the last error recorded in the global context.  The pointer is valid
 * until the next call that modifies the global error slot. */
SMC_API const smc_error_t *smc_last_error(void);

/* Return the last error recorded in an explicit context.  The pointer is valid
 * until the next call that modifies the same context's error slot. */
SMC_API const smc_error_t *smc_last_error_with(smc_context_t *ctx);

/* -------------------------------------------------------------------------- */
/* Observability                                                              */
/* -------------------------------------------------------------------------- */

/* Fill OUT with a snapshot of the current global statistics counters.
 * OUT must be non-NULL.  The counters are monotonically increasing for the
 * lifetime of the library (reset only by smc_reset_stats()). */
SMC_API int smc_get_stats(smc_stats_t *out);

/* Reset all global statistics counters to zero.  This is not thread-safe and
 * should not be called concurrently with smc_call_* or smc_eval_*. */
SMC_API int smc_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* SMC_H */
