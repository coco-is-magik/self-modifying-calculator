/* smc.h — stable ABI v1 for the Self-Modifying Calculator */
#ifndef SMC_H
#define SMC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMC_ABI_VERSION 1

#define SMC_OK             0
#define SMC_ERR_INIT      -1
#define SMC_ERR_PARSE     -2
#define SMC_ERR_EVAL      -3
#define SMC_ERR_NOT_IMPL  -4
#define SMC_ERR_IO        -5
#define SMC_ERR_INVALID   -6

typedef struct smc_context smc_context_t;
typedef uint32_t          smc_expr_id_t;

/* Error object. The layout is public and stable across ABI versions. */
struct smc_error {
    int   code;
    char  message[256];
};
typedef struct smc_error smc_error_t;

/* -------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

/* One-time library initialization. Must be called before any other API. */
int smc_init(void);

/* One-time library shutdown. Releases global resources. */
int smc_shutdown(void);

/* Create an isolated context with the given optimization level (1..3). */
smc_context_t *smc_context_create(int level);

/* Destroy a context created with smc_context_create(). */
void smc_context_destroy(smc_context_t *ctx);

/* -------------------------------------------------------------------------- */
/* Global context convenience API                                             */
/* -------------------------------------------------------------------------- */

/* Set the optimization level of the implicit global context. */
int smc_set_optimization_level(int level);

/* Get the optimization level of the implicit global context. */
int smc_get_optimization_level(void);

/* -------------------------------------------------------------------------- */
/* Tier 1: Development / tooling / embedded SBCL — expression strings       */
/* -------------------------------------------------------------------------- */

/* Evaluate a mathematical expression string and write the result to *out. */
int smc_eval_double(const char *expr, double *out);
int smc_eval_double_with(smc_context_t *ctx, const char *expr, double *out);

int smc_eval_float(const char *expr, float *out);
int smc_eval_float_with(smc_context_t *ctx, const char *expr, float *out);

int smc_eval_int(const char *expr, int64_t *out);
int smc_eval_int_with(smc_context_t *ctx, const char *expr, int64_t *out);

/* -------------------------------------------------------------------------- */
/* Tier 2: Production generated-code — expression IDs, no strings             */
/* -------------------------------------------------------------------------- */

/* Call a cached expression by stable ID, passing only its free variables. */
int smc_call_double(smc_expr_id_t expr_id,
                    const double *args, size_t argc,
                    double *out);

int smc_call_float(smc_expr_id_t expr_id,
                   const float *args, size_t argc,
                   float *out);

int smc_call_int(smc_expr_id_t expr_id,
                 const int64_t *args, size_t argc,
                 int64_t *out);

/* -------------------------------------------------------------------------- */
/* Variables                                                                  */
/* -------------------------------------------------------------------------- */

/* Bind a variable in the global context before evaluation. */
int smc_set_variable_double(const char *name, double value);
int smc_set_variable_double_with(smc_context_t *ctx, const char *name, double value);

/* Clear all variable bindings. */
int smc_clear_variables(void);
int smc_clear_variables_with(smc_context_t *ctx);

/* -------------------------------------------------------------------------- */
/* Cache control                                                              */
/* -------------------------------------------------------------------------- */

int smc_cache_clear(void);
int smc_cache_clear_with(smc_context_t *ctx);

int smc_cache_save(const char *path);
int smc_cache_save_with(smc_context_t *ctx, const char *path);

int smc_cache_load(const char *path);
int smc_cache_load_with(smc_context_t *ctx, const char *path);

/* -------------------------------------------------------------------------- */
/* Source generation (build-time optimizer output)                            */
/* -------------------------------------------------------------------------- */

/* Generate a C source file containing hot cached expressions. */
int smc_generate_c_source(const char *out_path);
int smc_generate_c_source_with(smc_context_t *ctx, const char *out_path);

/* -------------------------------------------------------------------------- */
/* Expression metadata (Tier 2)                                               */
/* -------------------------------------------------------------------------- */

/* Return the number of expressions available in the generated dispatch table. */
int smc_expr_count(void);

/* Return the arity (number of free variables) of expression ID. */
size_t smc_expr_arity(smc_expr_id_t id);

/* Return the original expression string for expression ID. */
const char *smc_expr_source(smc_expr_id_t id);

/* -------------------------------------------------------------------------- */
/* Error handling                                                             */
/* -------------------------------------------------------------------------- */

/* Return a human-readable string for an error code. */
const char *smc_error_string(int code);

/* Return the last error recorded in the global context. */
const smc_error_t *smc_last_error(void);

/* Return the last error recorded in an explicit context. */
const smc_error_t *smc_last_error_with(smc_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* SMC_H */
