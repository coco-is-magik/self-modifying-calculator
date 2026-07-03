/* smc_runtime_sbcl.c — optional SBCL-backed runtime for the Self-Modifying Calculator */
/*
 * This runtime links against an SBCL image saved with save-lisp-and-die and
 * exports the stable C ABI v1. It provides full SMC features: hierarchical
 * cache, runtime specialization, source rewriting, and arbitrary expression
 * evaluation.
 *
 * Building this runtime requires:
 *   1. An SBCL image with SMC loaded and a C-callable entry point registered.
 *   2. Compiling this file and linking it with the SBCL runtime libraries.
 *
 * For Milestone 1 this file is a documented placeholder. The actual SBCL
 * callback registration and image build are deferred to a later milestone
 * because they require non-trivial Lisp-side changes and build tooling.
 */

#include "smc.h"

#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/* Error codes                                                                */
/* -------------------------------------------------------------------------- */

#define SMC_OK             0
#define SMC_ERR_INIT      -1
#define SMC_ERR_NOT_IMPL  -4
#define SMC_ERR_INVALID   -6
#define SMC_ERR_ABI       -7
#define SMC_ERR_ARITY     -8
#define SMC_ERR_NOT_FOUND -9
#define SMC_ERR_THREAD    -10
#define SMC_ERR_SHUTDOWN  -11

/* -------------------------------------------------------------------------- */
/* Error state                                                                */
/* -------------------------------------------------------------------------- */

static smc_error_t g_last_error = {0, ""};

static void smc_set_error(int code, const char *msg) {
    g_last_error.code = code;
    if (msg) {
        strncpy(g_last_error.message, msg, sizeof(g_last_error.message) - 1);
        g_last_error.message[sizeof(g_last_error.message) - 1] = '\0';
    } else {
        g_last_error.message[0] = '\0';
    }
}

/* -------------------------------------------------------------------------- */
/* Placeholder implementation                                                 */
/* -------------------------------------------------------------------------- */

int smc_init(void) {
    smc_set_error(SMC_ERR_NOT_IMPL,
                  "SBCL-backed runtime is a build-time option not yet wired in Milestone 1");
    return SMC_ERR_NOT_IMPL;
}

int smc_shutdown(void) {
    return SMC_OK;
}

smc_context_t *smc_context_create(int level) {
    (void)level;
    smc_set_error(SMC_ERR_NOT_IMPL,
                  "SBCL-backed runtime is a build-time option not yet wired in Milestone 1");
    return NULL;
}

void smc_context_destroy(smc_context_t *ctx) {
    (void)ctx;
}

int smc_set_optimization_level(int level) {
    (void)level;
    smc_set_error(SMC_ERR_NOT_IMPL,
                  "SBCL-backed runtime is a build-time option not yet wired in Milestone 1");
    return SMC_ERR_NOT_IMPL;
}

int smc_get_optimization_level(void) {
    return 0;
}

int smc_eval_double(const char *expr, double *out) {
    (void)expr;
    (void)out;
    smc_set_error(SMC_ERR_NOT_IMPL,
                  "SBCL-backed runtime is a build-time option not yet wired in Milestone 1");
    return SMC_ERR_NOT_IMPL;
}

int smc_eval_double_with(smc_context_t *ctx, const char *expr, double *out) {
    (void)ctx;
    (void)expr;
    (void)out;
    smc_set_error(SMC_ERR_NOT_IMPL,
                  "SBCL-backed runtime is a build-time option not yet wired in Milestone 1");
    return SMC_ERR_NOT_IMPL;
}

int smc_eval_float(const char *expr, float *out) {
    (void)expr;
    (void)out;
    smc_set_error(SMC_ERR_NOT_IMPL,
                  "SBCL-backed runtime is a build-time option not yet wired in Milestone 1");
    return SMC_ERR_NOT_IMPL;
}

int smc_eval_float_with(smc_context_t *ctx, const char *expr, float *out) {
    (void)ctx;
    (void)expr;
    (void)out;
    smc_set_error(SMC_ERR_NOT_IMPL,
                  "SBCL-backed runtime is a build-time option not yet wired in Milestone 1");
    return SMC_ERR_NOT_IMPL;
}

int smc_eval_int(const char *expr, int64_t *out) {
    (void)expr;
    (void)out;
    smc_set_error(SMC_ERR_NOT_IMPL,
                  "SBCL-backed runtime is a build-time option not yet wired in Milestone 1");
    return SMC_ERR_NOT_IMPL;
}

int smc_eval_int_with(smc_context_t *ctx, const char *expr, int64_t *out) {
    (void)ctx;
    (void)expr;
    (void)out;
    smc_set_error(SMC_ERR_NOT_IMPL,
                  "SBCL-backed runtime is a build-time option not yet wired in Milestone 1");
    return SMC_ERR_NOT_IMPL;
}

int smc_call_double(smc_expr_id_t expr_id, const double *args, size_t argc, double *out) {
    (void)expr_id;
    (void)args;
    (void)argc;
    (void)out;
    smc_set_error(SMC_ERR_NOT_IMPL,
                  "SBCL-backed runtime is a build-time option not yet wired in Milestone 1");
    return SMC_ERR_NOT_IMPL;
}

int smc_call_float(smc_expr_id_t expr_id, const float *args, size_t argc, float *out) {
    (void)expr_id;
    (void)args;
    (void)argc;
    (void)out;
    smc_set_error(SMC_ERR_NOT_IMPL,
                  "SBCL-backed runtime is a build-time option not yet wired in Milestone 1");
    return SMC_ERR_NOT_IMPL;
}

int smc_call_int(smc_expr_id_t expr_id, const int64_t *args, size_t argc, int64_t *out) {
    (void)expr_id;
    (void)args;
    (void)argc;
    (void)out;
    smc_set_error(SMC_ERR_NOT_IMPL,
                  "SBCL-backed runtime is a build-time option not yet wired in Milestone 1");
    return SMC_ERR_NOT_IMPL;
}

int smc_set_variable_double(const char *name, double value) {
    (void)name;
    (void)value;
    smc_set_error(SMC_ERR_NOT_IMPL,
                  "SBCL-backed runtime is a build-time option not yet wired in Milestone 1");
    return SMC_ERR_NOT_IMPL;
}

int smc_set_variable_double_with(smc_context_t *ctx, const char *name, double value) {
    (void)ctx;
    (void)name;
    (void)value;
    smc_set_error(SMC_ERR_NOT_IMPL,
                  "SBCL-backed runtime is a build-time option not yet wired in Milestone 1");
    return SMC_ERR_NOT_IMPL;
}

int smc_clear_variables(void) {
    smc_set_error(SMC_ERR_NOT_IMPL,
                  "SBCL-backed runtime is a build-time option not yet wired in Milestone 1");
    return SMC_ERR_NOT_IMPL;
}

int smc_clear_variables_with(smc_context_t *ctx) {
    (void)ctx;
    smc_set_error(SMC_ERR_NOT_IMPL,
                  "SBCL-backed runtime is a build-time option not yet wired in Milestone 1");
    return SMC_ERR_NOT_IMPL;
}

int smc_cache_clear(void) {
    return SMC_OK;
}

int smc_cache_clear_with(smc_context_t *ctx) {
    (void)ctx;
    return SMC_OK;
}

int smc_cache_save(const char *path) {
    (void)path;
    smc_set_error(SMC_ERR_NOT_IMPL,
                  "SBCL-backed runtime is a build-time option not yet wired in Milestone 1");
    return SMC_ERR_NOT_IMPL;
}

int smc_cache_save_with(smc_context_t *ctx, const char *path) {
    (void)ctx;
    (void)path;
    smc_set_error(SMC_ERR_NOT_IMPL,
                  "SBCL-backed runtime is a build-time option not yet wired in Milestone 1");
    return SMC_ERR_NOT_IMPL;
}

int smc_cache_load(const char *path) {
    (void)path;
    smc_set_error(SMC_ERR_NOT_IMPL,
                  "SBCL-backed runtime is a build-time option not yet wired in Milestone 1");
    return SMC_ERR_NOT_IMPL;
}

int smc_cache_load_with(smc_context_t *ctx, const char *path) {
    (void)ctx;
    (void)path;
    smc_set_error(SMC_ERR_NOT_IMPL,
                  "SBCL-backed runtime is a build-time option not yet wired in Milestone 1");
    return SMC_ERR_NOT_IMPL;
}

int smc_generate_c_source(const char *out_path) {
    (void)out_path;
    smc_set_error(SMC_ERR_NOT_IMPL,
                  "SBCL-backed runtime is a build-time option not yet wired in Milestone 1");
    return SMC_ERR_NOT_IMPL;
}

int smc_generate_c_source_with(smc_context_t *ctx, const char *out_path) {
    (void)ctx;
    (void)out_path;
    smc_set_error(SMC_ERR_NOT_IMPL,
                  "SBCL-backed runtime is a build-time option not yet wired in Milestone 1");
    return SMC_ERR_NOT_IMPL;
}

int smc_abi_version(void) {
    return SMC_ABI_VERSION;
}

const char *smc_runtime_kind(void) {
    return "sbcl";
}

int smc_expr_count(void) {
    return 0;
}

size_t smc_expr_arity(smc_expr_id_t id) {
    (void)id;
    return 0;
}

const char *smc_expr_source(smc_expr_id_t id) {
    (void)id;
    return NULL;
}

const char *smc_error_string(int code) {
    switch (code) {
        case SMC_OK:            return "success";
        case SMC_ERR_INIT:      return "initialization error";
        case SMC_ERR_NOT_IMPL:  return "not implemented";
        case SMC_ERR_INVALID:   return "invalid argument";
        case SMC_ERR_ABI:       return "ABI version mismatch";
        case SMC_ERR_ARITY:     return "wrong arity";
        case SMC_ERR_NOT_FOUND: return "expression not found";
        case SMC_ERR_THREAD:    return "thread-safety violation";
        case SMC_ERR_SHUTDOWN:  return "library shut down";
        default:                return "unknown error";
    }
}

const smc_error_t *smc_last_error(void) {
    return (const smc_error_t *)&g_last_error;
}

const smc_error_t *smc_last_error_with(smc_context_t *ctx) {
    (void)ctx;
    return (const smc_error_t *)&g_last_error;
}

/* -------------------------------------------------------------------------- */
/* Observability                                                              */
/* -------------------------------------------------------------------------- */

static smc_stats_t g_sbcl_stats = {0, 0, 0, 0, 0, 0, 0, 0};

int smc_get_stats(smc_stats_t *out) {
    if (!out) {
        smc_set_error(SMC_ERR_INVALID, "null argument");
        return SMC_ERR_INVALID;
    }
    *out = g_sbcl_stats;
    return SMC_OK;
}

int smc_reset_stats(void) {
    memset(&g_sbcl_stats, 0, sizeof(g_sbcl_stats));
    return SMC_OK;
}
