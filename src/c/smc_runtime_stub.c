/* smc_runtime_stub.c — standalone C runtime for the Self-Modifying Calculator
 *
 * This is the default runtime. It implements the stable C API without
 * any dependency on SBCL. It contains a tiny recursive-descent parser
 * and evaluator for scalar arithmetic expressions.
 *
 * This file also provides the v2 artifact cache and dirty-state APIs.
 */

#include "smc.h"
#include "smc_artifact.h"
#include "smc_state.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef SMC_THREAD_SAFE
#include <pthread.h>
static pthread_mutex_t g_global_mutex = PTHREAD_MUTEX_INITIALIZER;
#define SMC_LOCK_GLOBAL()   pthread_mutex_lock(&g_global_mutex)
#define SMC_UNLOCK_GLOBAL() pthread_mutex_unlock(&g_global_mutex)
#else
#define SMC_LOCK_GLOBAL()
#define SMC_UNLOCK_GLOBAL()
#endif

/* -------------------------------------------------------------------------- */
/* Error state                                                                */
/* -------------------------------------------------------------------------- */

static smc_error_t g_last_error = {0, ""};

/* -------------------------------------------------------------------------- */
/* Statistics counters                                                        */
/* -------------------------------------------------------------------------- */

SMC_API smc_stats_t smc_global_stats = {0, 0, 0, 0, 0, 0, 0, 0};

static smc_stats_t *g_stats = &smc_global_stats;

static void smc_stats_record_error(int code) {
    g_stats->last_error_code = code;
}

static void smc_stats_increment(uint64_t *counter) {
    (*counter)++;
}

static void smc_set_error(int code, const char *msg) {
    g_last_error.code = code;
    smc_stats_record_error(code);
    if (msg) {
        strncpy(g_last_error.message, msg, sizeof(g_last_error.message) - 1);
        g_last_error.message[sizeof(g_last_error.message) - 1] = '\0';
    } else {
        g_last_error.message[0] = '\0';
    }
}

static void smc_set_errorf(int code, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    g_last_error.code = code;
    smc_stats_record_error(code);
    vsnprintf(g_last_error.message, sizeof(g_last_error.message), fmt, ap);
    g_last_error.message[sizeof(g_last_error.message) - 1] = '\0';
    va_end(ap);
}

/* -------------------------------------------------------------------------- */
/* Variables                                                                  */
/* -------------------------------------------------------------------------- */

#define SMC_MAX_VARIABLES 64

typedef struct {
    char   name[64];
    double value;
} smc_variable_t;

struct smc_variable_table {
    smc_variable_t entries[SMC_MAX_VARIABLES];
    size_t         count;
};

static void smc_variable_table_init(struct smc_variable_table *vt) {
    vt->count = 0;
}

static int smc_variable_table_set(struct smc_variable_table *vt,
                                   const char *name, double value) {
    if (!name) {
        return SMC_ERR_INVALID;
    }
    size_t len = strlen(name);
    if (len == 0 || len >= sizeof(vt->entries[0].name)) {
        return SMC_ERR_INVALID;
    }
    for (size_t i = 0; i < vt->count; i++) {
        if (strcmp(vt->entries[i].name, name) == 0) {
            vt->entries[i].value = value;
            return SMC_OK;
        }
    }
    if (vt->count >= SMC_MAX_VARIABLES) {
        return SMC_ERR_EVAL;
    }
    memcpy(vt->entries[vt->count].name, name, len + 1);
    vt->entries[vt->count].value = value;
    vt->count++;
    return SMC_OK;
}

static int smc_variable_table_get(struct smc_variable_table *vt,
                                   const char *name, double *out) {
    if (!name || !out) {
        return SMC_ERR_INVALID;
    }
    for (size_t i = 0; i < vt->count; i++) {
        if (strcmp(vt->entries[i].name, name) == 0) {
            *out = vt->entries[i].value;
            return SMC_OK;
        }
    }
    return SMC_ERR_EVAL;
}

static void smc_variable_table_clear(struct smc_variable_table *vt) {
    vt->count = 0;
}

/* -------------------------------------------------------------------------- */
/* Context                                                                    */
/* -------------------------------------------------------------------------- */

struct smc_context {
    int   level;
    int   initialized;
    struct smc_variable_table variables;
    smc_artifact_table_t artifact_table;
    smc_state_table_t state_table;
    smc_indexed_state_table_t indexed_state_table;
    smc_artifact_stats_t artifact_stats;
    smc_state_stats_t state_stats;
    smc_state_indexed_stats_t indexed_state_stats;
    int artifact_configured;
    int state_configured;
    int indexed_state_configured;
};

static smc_context_t g_global_context = {0};
static int           g_initialized    = 0;

/* -------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

int smc_init(void) {
    SMC_LOCK_GLOBAL();
    if (g_initialized) {
        SMC_UNLOCK_GLOBAL();
        return SMC_OK;
    }

    extern int smc_generated_abi_version(void) __attribute__((weak));
    if (smc_generated_abi_version) {
        int generated_abi = smc_generated_abi_version();
        if (generated_abi != 0 && generated_abi != SMC_ABI_VERSION) {
            smc_set_errorf(SMC_ERR_ABI,
                           "generated ABI version %d does not match runtime ABI version %d",
                           generated_abi, SMC_ABI_VERSION);
            SMC_UNLOCK_GLOBAL();
            return SMC_ERR_ABI;
        }
    }

    g_initialized = 1;
    g_global_context.level = 1;
    g_global_context.initialized = 1;
    smc_variable_table_init(&g_global_context.variables);
    g_global_context.artifact_configured = 0;
    g_global_context.state_configured = 0;
    g_global_context.indexed_state_configured = 0;
    smc_set_error(SMC_OK, NULL);
    SMC_UNLOCK_GLOBAL();
    return SMC_OK;
}

int smc_shutdown(void) {
    SMC_LOCK_GLOBAL();
    g_initialized = 0;
    g_global_context.initialized = 0;
    SMC_UNLOCK_GLOBAL();
    return SMC_OK;
}

smc_context_t *smc_context_create(int level) {
    smc_context_t *ctx = (smc_context_t *)calloc(1, sizeof(smc_context_t));
    if (!ctx) {
        smc_set_error(SMC_ERR_INIT, "failed to allocate context");
        return NULL;
    }
    ctx->level = (level < 1) ? 1 : (level > 3 ? 3 : level);
    ctx->initialized = 1;
    smc_variable_table_init(&ctx->variables);
    ctx->artifact_configured = 0;
    ctx->state_configured = 0;
    ctx->indexed_state_configured = 0;
    memset(&ctx->artifact_stats, 0, sizeof(ctx->artifact_stats));
    memset(&ctx->state_stats, 0, sizeof(ctx->state_stats));
    memset(&ctx->indexed_state_stats, 0, sizeof(ctx->indexed_state_stats));
    return ctx;
}

void smc_context_destroy(smc_context_t *ctx) {
    if (ctx) {
        smc_artifact_table_destroy(&ctx->artifact_table);
        smc_state_table_destroy(&ctx->state_table);
        smc_indexed_state_table_destroy(&ctx->indexed_state_table);
        free(ctx);
    }
}

/* -------------------------------------------------------------------------- */
/* Global context convenience API                                             */
/* -------------------------------------------------------------------------- */

int smc_set_optimization_level(int level) {
    SMC_LOCK_GLOBAL();
    if (!g_initialized) {
        smc_set_error(SMC_ERR_INIT, "library not initialized");
        SMC_UNLOCK_GLOBAL();
        return SMC_ERR_INIT;
    }
    g_global_context.level = (level < 1) ? 1 : (level > 3 ? 3 : level);
    SMC_UNLOCK_GLOBAL();
    return SMC_OK;
}

int smc_get_optimization_level(void) {
    SMC_LOCK_GLOBAL();
    if (!g_initialized) {
        SMC_UNLOCK_GLOBAL();
        return 0;
    }
    int level = g_global_context.level;
    SMC_UNLOCK_GLOBAL();
    return level;
}

/* -------------------------------------------------------------------------- */
/* Tier 1 expression parser                                                   */
/* -------------------------------------------------------------------------- */

typedef struct {
    const char *s;
    size_t      pos;
    size_t      len;
} smc_parser_t;

static void smc_parser_init(smc_parser_t *p, const char *expr) {
    p->s   = expr;
    p->pos = 0;
    p->len = strlen(expr);
}

static void smc_parser_skip_ws(smc_parser_t *p) {
    while (p->pos < p->len && isspace((unsigned char)p->s[p->pos])) {
        p->pos++;
    }
}

static int smc_parser_peek(smc_parser_t *p) {
    smc_parser_skip_ws(p);
    if (p->pos >= p->len) {
        return -1;
    }
    return (unsigned char)p->s[p->pos];
}

static int smc_parser_get(smc_parser_t *p) {
    smc_parser_skip_ws(p);
    if (p->pos >= p->len) {
        return -1;
    }
    return (unsigned char)p->s[p->pos++];
}

static int smc_parser_expect(smc_parser_t *p, char c) {
    int ch = smc_parser_get(p);
    if (ch != c) {
        smc_set_errorf(SMC_ERR_PARSE, "expected '%c' at position %zu", c, p->pos);
        return SMC_ERR_PARSE;
    }
    return SMC_OK;
}

static int smc_parse_expression(smc_parser_t *p, smc_context_t *ctx, double *out);

static int smc_parse_number(smc_parser_t *p, double *out) {
    smc_parser_skip_ws(p);
    size_t start = p->pos;
    int has_dot = 0;

    while (p->pos < p->len) {
        char c = p->s[p->pos];
        if (c == '.') {
            if (has_dot) break;
            has_dot = 1;
            p->pos++;
        } else if (isdigit((unsigned char)c)) {
            p->pos++;
        } else {
            break;
        }
    }

    if (start == p->pos) {
        smc_set_errorf(SMC_ERR_PARSE, "expected number at position %zu", p->pos);
        return SMC_ERR_PARSE;
    }

    char *buf = (char *)malloc(p->pos - start + 1);
    if (!buf) {
        smc_set_error(SMC_ERR_INIT, "out of memory");
        return SMC_ERR_INIT;
    }
    memcpy(buf, p->s + start, p->pos - start);
    buf[p->pos - start] = '\0';
    *out = strtod(buf, NULL);
    free(buf);
    return SMC_OK;
}

static int smc_parse_identifier(smc_parser_t *p, char *out, size_t out_size) {
    smc_parser_skip_ws(p);
    size_t start = p->pos;
    if (start >= p->len || !isalpha((unsigned char)p->s[start])) {
        smc_set_errorf(SMC_ERR_PARSE, "expected identifier at position %zu", p->pos);
        return SMC_ERR_PARSE;
    }
    while (p->pos < p->len && 
           (isalnum((unsigned char)p->s[p->pos]) || p->s[p->pos] == '_')) {
        p->pos++;
    }
    size_t len = p->pos - start;
    if (len >= out_size) len = out_size - 1;
    memcpy(out, p->s + start, len);
    out[len] = '\0';
    return SMC_OK;
}

static int smc_parse_primary(smc_parser_t *p, smc_context_t *ctx, double *out) {
    smc_parser_skip_ws(p);
    int c = smc_parser_peek(p);
    if (c < 0) {
        smc_set_error(SMC_ERR_PARSE, "unexpected end of expression");
        return SMC_ERR_PARSE;
    }

    if (c == '(') {
        smc_parser_get(p);
        int rc = smc_parse_expression(p, ctx, out);
        if (rc != SMC_OK) return rc;
        return smc_parser_expect(p, ')');
    }

    if (isdigit((unsigned char)c) || c == '.') {
        return smc_parse_number(p, out);
    }

    if (isalpha((unsigned char)c)) {
        char name[64];
        int rc = smc_parse_identifier(p, name, sizeof(name));
        if (rc != SMC_OK) return rc;
        rc = smc_variable_table_get(&ctx->variables, name, out);
        if (rc != SMC_OK) {
            smc_set_errorf(SMC_ERR_EVAL, "unbound variable '%s'", name);
        }
        return rc;
    }

    smc_set_errorf(SMC_ERR_PARSE, "unexpected character '%c' at position %zu", c, p->pos);
    return SMC_ERR_PARSE;
}

static int smc_parse_power(smc_parser_t *p, smc_context_t *ctx, double *out) {
    int rc = smc_parse_primary(p, ctx, out);
    if (rc != SMC_OK) return rc;
    if (smc_parser_peek(p) == '^') {
        smc_parser_get(p);
        double rhs;
        rc = smc_parse_power(p, ctx, &rhs);
        if (rc != SMC_OK) return rc;
        *out = pow(*out, rhs);
    }
    return SMC_OK;
}

static int smc_parse_unary(smc_parser_t *p, smc_context_t *ctx, double *out) {
    int c = smc_parser_peek(p);
    if (c == '+' || c == '-') {
        smc_parser_get(p);
        int rc = smc_parse_unary(p, ctx, out);
        if (rc != SMC_OK) return rc;
        if (c == '-') *out = -(*out);
        return SMC_OK;
    }
    return smc_parse_power(p, ctx, out);
}

static int smc_parse_term(smc_parser_t *p, smc_context_t *ctx, double *out) {
    int rc = smc_parse_unary(p, ctx, out);
    if (rc != SMC_OK) return rc;
    for (;;) {
        int c = smc_parser_peek(p);
        if (c != '*' && c != '/') break;
        smc_parser_get(p);
        double rhs;
        rc = smc_parse_unary(p, ctx, &rhs);
        if (rc != SMC_OK) return rc;
        if (c == '*') *out *= rhs;
        else {
            if (rhs == 0.0) {
                smc_set_error(SMC_ERR_EVAL, "division by zero");
                return SMC_ERR_EVAL;
            }
            *out /= rhs;
        }
    }
    return SMC_OK;
}

static int smc_parse_expression(smc_parser_t *p, smc_context_t *ctx, double *out) {
    int rc = smc_parse_term(p, ctx, out);
    if (rc != SMC_OK) return rc;
    for (;;) {
        int c = smc_parser_peek(p);
        if (c != '+' && c != '-') break;
        smc_parser_get(p);
        double rhs;
        rc = smc_parse_term(p, ctx, &rhs);
        if (rc != SMC_OK) return rc;
        if (c == '+') *out += rhs;
        else *out -= rhs;
    }
    return SMC_OK;
}

/* -------------------------------------------------------------------------- */
/* Tier 1 evaluation                                                          */
/* -------------------------------------------------------------------------- */

static int smc_eval_double_impl(smc_context_t *ctx, const char *expr, double *out) {
    if (!g_initialized) {
        smc_set_error(SMC_ERR_INIT, "library not initialized");
        return SMC_ERR_INIT;
    }
    if (!expr || !out) {
        smc_set_error(SMC_ERR_INVALID, "null argument");
        return SMC_ERR_INVALID;
    }

    smc_parser_t p;
    smc_parser_init(&p, expr);
    int rc = smc_parse_expression(&p, ctx, out);
    if (rc != SMC_OK) {
        smc_stats_increment(&g_stats->parse_errors);
        return rc;
    }
    if (smc_parser_peek(&p) >= 0) {
        smc_set_error(SMC_ERR_PARSE, "trailing characters in expression");
        smc_stats_increment(&g_stats->parse_errors);
        return SMC_ERR_PARSE;
    }
    return SMC_OK;
}

int smc_eval_double(const char *expr, double *out) {
    SMC_LOCK_GLOBAL();
    int rc = smc_eval_double_impl(&g_global_context, expr, out);
    SMC_UNLOCK_GLOBAL();
    return rc;
}

int smc_eval_double_with(smc_context_t *ctx, const char *expr, double *out) {
    if (!ctx || !ctx->initialized) {
        smc_set_error(SMC_ERR_INIT, "invalid context");
        return SMC_ERR_INIT;
    }
    return smc_eval_double_impl(ctx, expr, out);
}

int smc_eval_float(const char *expr, float *out) {
    double d;
    int rc = smc_eval_double(expr, &d);
    if (rc == SMC_OK && out) *out = (float)d;
    return rc;
}

int smc_eval_float_with(smc_context_t *ctx, const char *expr, float *out) {
    double d;
    int rc = smc_eval_double_with(ctx, expr, &d);
    if (rc == SMC_OK && out) *out = (float)d;
    return rc;
}

int smc_eval_int(const char *expr, int64_t *out) {
    double d;
    int rc = smc_eval_double(expr, &d);
    if (rc == SMC_OK && out) *out = (int64_t)d;
    return rc;
}

int smc_eval_int_with(smc_context_t *ctx, const char *expr, int64_t *out) {
    double d;
    int rc = smc_eval_double_with(ctx, expr, &d);
    if (rc == SMC_OK && out) *out = (int64_t)d;
    return rc;
}

/* -------------------------------------------------------------------------- */
/* Tier 2 generated-code calls (weak, overridden by generated table)          */
/* -------------------------------------------------------------------------- */

__attribute__((weak)) int smc_call_double(smc_expr_id_t expr_id,
                     const double *args, size_t argc, double *out) {
    (void)expr_id; (void)args; (void)argc; (void)out;
    smc_stats_increment(&g_stats->total_calls);
    smc_stats_increment(&g_stats->fallback_evals);
    smc_set_error(SMC_ERR_NOT_IMPL, "no generated dispatch table linked");
    return SMC_ERR_NOT_IMPL;
}

__attribute__((weak)) int smc_call_float(smc_expr_id_t expr_id,
                    const float *args, size_t argc, float *out) {
    (void)expr_id; (void)args; (void)argc; (void)out;
    smc_stats_increment(&g_stats->total_calls);
    smc_stats_increment(&g_stats->fallback_evals);
    smc_set_error(SMC_ERR_NOT_IMPL, "no generated dispatch table linked");
    return SMC_ERR_NOT_IMPL;
}

__attribute__((weak)) int smc_call_int(smc_expr_id_t expr_id,
                 const int64_t *args, size_t argc, int64_t *out) {
    (void)expr_id; (void)args; (void)argc; (void)out;
    smc_stats_increment(&g_stats->total_calls);
    smc_stats_increment(&g_stats->fallback_evals);
    smc_set_error(SMC_ERR_NOT_IMPL, "no generated dispatch table linked");
    return SMC_ERR_NOT_IMPL;
}

__attribute__((weak)) int smc_expr_count(void) {
    return 0;
}

__attribute__((weak)) size_t smc_expr_arity(smc_expr_id_t id) {
    (void)id;
    return 0;
}

__attribute__((weak)) const char *smc_expr_source(smc_expr_id_t id) {
    (void)id;
    return NULL;
}

/* -------------------------------------------------------------------------- */
/* Variables                                                                  */
/* -------------------------------------------------------------------------- */

int smc_set_variable_double(const char *name, double value) {
    SMC_LOCK_GLOBAL();
    if (!g_initialized) {
        smc_set_error(SMC_ERR_INIT, "library not initialized");
        SMC_UNLOCK_GLOBAL();
        return SMC_ERR_INIT;
    }
    int rc = smc_variable_table_set(&g_global_context.variables, name, value);
    SMC_UNLOCK_GLOBAL();
    return rc;
}

int smc_set_variable_double_with(smc_context_t *ctx, const char *name, double value) {
    if (!ctx || !ctx->initialized) {
        smc_set_error(SMC_ERR_INIT, "invalid context");
        return SMC_ERR_INIT;
    }
    return smc_variable_table_set(&ctx->variables, name, value);
}

int smc_clear_variables(void) {
    SMC_LOCK_GLOBAL();
    if (!g_initialized) {
        smc_set_error(SMC_ERR_INIT, "library not initialized");
        SMC_UNLOCK_GLOBAL();
        return SMC_ERR_INIT;
    }
    smc_variable_table_clear(&g_global_context.variables);
    SMC_UNLOCK_GLOBAL();
    return SMC_OK;
}

int smc_clear_variables_with(smc_context_t *ctx) {
    if (!ctx || !ctx->initialized) {
        smc_set_error(SMC_ERR_INIT, "invalid context");
        return SMC_ERR_INIT;
    }
    smc_variable_table_clear(&ctx->variables);
    return SMC_OK;
}

/* -------------------------------------------------------------------------- */
/* Cache control                                                              */
/* -------------------------------------------------------------------------- */

int smc_cache_clear(void) {
    return SMC_OK;
}

int smc_cache_clear_with(smc_context_t *ctx) {
    (void)ctx;
    return SMC_OK;
}

int smc_cache_save(const char *path) {
    (void)path;
    smc_set_error(SMC_ERR_NOT_IMPL, "cache persistence not implemented in stub runtime");
    return SMC_ERR_NOT_IMPL;
}

int smc_cache_save_with(smc_context_t *ctx, const char *path) {
    (void)ctx; (void)path;
    smc_set_error(SMC_ERR_NOT_IMPL, "cache persistence not implemented in stub runtime");
    return SMC_ERR_NOT_IMPL;
}

int smc_cache_load(const char *path) {
    (void)path;
    smc_set_error(SMC_ERR_NOT_IMPL, "cache persistence not implemented in stub runtime");
    return SMC_ERR_NOT_IMPL;
}

int smc_cache_load_with(smc_context_t *ctx, const char *path) {
    (void)ctx; (void)path;
    smc_set_error(SMC_ERR_NOT_IMPL, "cache persistence not implemented in stub runtime");
    return SMC_ERR_NOT_IMPL;
}

/* -------------------------------------------------------------------------- */
/* Source generation                                                          */
/* -------------------------------------------------------------------------- */

int smc_generate_c_source(const char *out_path) {
    (void)out_path;
    smc_set_error(SMC_ERR_NOT_IMPL, "source generation not implemented in stub runtime");
    return SMC_ERR_NOT_IMPL;
}

int smc_generate_c_source_with(smc_context_t *ctx, const char *out_path) {
    (void)ctx; (void)out_path;
    smc_set_error(SMC_ERR_NOT_IMPL, "source generation not implemented in stub runtime");
    return SMC_ERR_NOT_IMPL;
}

/* -------------------------------------------------------------------------- */
/* Artifact cache (ABI v2)                                                    */
/* -------------------------------------------------------------------------- */

int smc_artifact_configure(smc_context_t *ctx, const smc_artifact_config_t *config) {
    if (!ctx || !ctx->initialized) {
        smc_set_error(SMC_ERR_INIT, "invalid context");
        return SMC_ERR_INIT;
    }
    if (!config) {
        smc_set_error(SMC_ERR_INVALID, "null config");
        return SMC_ERR_INVALID;
    }
    if (ctx->artifact_configured) {
        smc_set_error(SMC_ERR_INVALID, "artifact cache already configured");
        return SMC_ERR_INVALID;
    }
    
    int rc = smc_artifact_table_init(&ctx->artifact_table, config, &ctx->artifact_stats);
    if (rc == SMC_OK) {
        ctx->artifact_configured = 1;
    }
    return rc;
}

int smc_artifact_lookup(smc_context_t *ctx,
                         const void *key, size_t key_size,
                         void *out_value, size_t value_capacity,
                         size_t *out_value_size) {
    if (!ctx || !ctx->initialized) {
        smc_set_error(SMC_ERR_INIT, "invalid context");
        return SMC_ERR_INIT;
    }
    if (!ctx->artifact_configured) {
        smc_set_error(SMC_ERR_INIT, "artifact cache not configured");
        return SMC_ERR_INIT;
    }
    return smc_artifact_table_lookup(&ctx->artifact_table, key, key_size,
                                      out_value, value_capacity, out_value_size);
}

int smc_artifact_store(smc_context_t *ctx,
                        const void *key, size_t key_size,
                        const void *value, size_t value_size) {
    if (!ctx || !ctx->initialized) {
        smc_set_error(SMC_ERR_INIT, "invalid context");
        return SMC_ERR_INIT;
    }
    if (!ctx->artifact_configured) {
        smc_set_error(SMC_ERR_INIT, "artifact cache not configured");
        return SMC_ERR_INIT;
    }
    return smc_artifact_table_store(&ctx->artifact_table, key, key_size, value, value_size);
}

int smc_artifact_remove(smc_context_t *ctx,
                         const void *key, size_t key_size) {
    if (!ctx || !ctx->initialized) {
        smc_set_error(SMC_ERR_INIT, "invalid context");
        return SMC_ERR_INIT;
    }
    if (!ctx->artifact_configured) {
        smc_set_error(SMC_ERR_INIT, "artifact cache not configured");
        return SMC_ERR_INIT;
    }
    return smc_artifact_table_remove(&ctx->artifact_table, key, key_size);
}

int smc_artifact_clear(smc_context_t *ctx) {
    if (!ctx || !ctx->initialized) {
        smc_set_error(SMC_ERR_INIT, "invalid context");
        return SMC_ERR_INIT;
    }
    if (!ctx->artifact_configured) {
        smc_set_error(SMC_ERR_INIT, "artifact cache not configured");
        return SMC_ERR_INIT;
    }
    return smc_artifact_table_clear(&ctx->artifact_table);
}

int smc_artifact_get_stats(smc_context_t *ctx, smc_artifact_stats_t *out) {
    if (!ctx || !ctx->initialized) {
        smc_set_error(SMC_ERR_INIT, "invalid context");
        return SMC_ERR_INIT;
    }
    if (!out) {
        smc_set_error(SMC_ERR_INVALID, "null stats output");
        return SMC_ERR_INVALID;
    }
    *out = ctx->artifact_stats;
    return SMC_OK;
}

int smc_artifact_reset_stats(smc_context_t *ctx) {
    if (!ctx || !ctx->initialized) {
        smc_set_error(SMC_ERR_INIT, "invalid context");
        return SMC_ERR_INIT;
    }
    memset(&ctx->artifact_stats, 0, sizeof(ctx->artifact_stats));
    return SMC_OK;
}

/* -------------------------------------------------------------------------- */
/* Dirty-state tracking (ABI v2)                                              */
/* -------------------------------------------------------------------------- */

int smc_state_configure(smc_context_t *ctx, const smc_state_config_t *config) {
    if (!ctx || !ctx->initialized) {
        smc_set_error(SMC_ERR_INIT, "invalid context");
        return SMC_ERR_INIT;
    }
    if (!config) {
        smc_set_error(SMC_ERR_INVALID, "null config");
        return SMC_ERR_INVALID;
    }
    if (ctx->state_configured) {
        smc_set_error(SMC_ERR_INVALID, "state tracker already configured");
        return SMC_ERR_INVALID;
    }
    
    int rc = smc_state_table_init(&ctx->state_table, config, &ctx->state_stats);
    if (rc == SMC_OK) {
        ctx->state_configured = 1;
    }
    return rc;
}

int smc_state_changed(smc_context_t *ctx,
                       const void *key, size_t key_size,
                       const void *state, size_t state_size,
                       int *out_changed) {
    if (!ctx || !ctx->initialized) {
        smc_set_error(SMC_ERR_INIT, "invalid context");
        return SMC_ERR_INIT;
    }
    if (!ctx->state_configured) {
        smc_set_error(SMC_ERR_INIT, "state tracker not configured");
        return SMC_ERR_INIT;
    }
    return smc_state_table_check(&ctx->state_table, key, key_size, state, state_size, out_changed);
}

int smc_state_clear(smc_context_t *ctx) {
    if (!ctx || !ctx->initialized) {
        smc_set_error(SMC_ERR_INIT, "invalid context");
        return SMC_ERR_INIT;
    }
    if (!ctx->state_configured) {
        smc_set_error(SMC_ERR_INIT, "state tracker not configured");
        return SMC_ERR_INIT;
    }
    return smc_state_table_clear(&ctx->state_table);
}

int smc_state_get_stats(smc_context_t *ctx, smc_state_stats_t *out) {
    if (!ctx || !ctx->initialized) {
        smc_set_error(SMC_ERR_INIT, "invalid context");
        return SMC_ERR_INIT;
    }
    if (!out) {
        smc_set_error(SMC_ERR_INVALID, "null stats output");
        return SMC_ERR_INVALID;
    }
    *out = ctx->state_stats;
    return SMC_OK;
}

int smc_state_reset_stats(smc_context_t *ctx) {
    if (!ctx || !ctx->initialized) {
        smc_set_error(SMC_ERR_INIT, "invalid context");
        return SMC_ERR_INIT;
    }
    memset(&ctx->state_stats, 0, sizeof(ctx->state_stats));
    return SMC_OK;
}

/* -------------------------------------------------------------------------- */
/* Indexed-state tracking (ABI v2.1)                                              */
/* -------------------------------------------------------------------------- */

int smc_state_indexed_configure(smc_context_t *ctx,
                                 const smc_state_indexed_config_t *config) {
    if (!ctx || !ctx->initialized) {
        smc_set_error(SMC_ERR_INIT, "invalid context");
        return SMC_ERR_INIT;
    }
    if (!config) {
        smc_set_error(SMC_ERR_INVALID, "null config");
        return SMC_ERR_INVALID;
    }
    if (ctx->indexed_state_configured) {
        smc_set_error(SMC_ERR_INVALID, "indexed state tracker already configured");
        return SMC_ERR_INVALID;
    }
    
    int rc = smc_indexed_state_table_init(&ctx->indexed_state_table, config,
                                           &ctx->indexed_state_stats);
    if (rc == SMC_OK) {
        ctx->indexed_state_configured = 1;
    }
    return rc;
}

int smc_state_changed_index(smc_context_t *ctx,
                              uint32_t index,
                              const void *state,
                              size_t state_size,
                              int *out_changed) {
    if (!ctx || !ctx->initialized) {
        smc_set_error(SMC_ERR_INIT, "invalid context");
        return SMC_ERR_INIT;
    }
    if (!ctx->indexed_state_configured) {
        smc_set_error(SMC_ERR_INIT, "indexed state tracker not configured");
        return SMC_ERR_INIT;
    }
    return smc_indexed_state_table_check(&ctx->indexed_state_table, index,
                                          state, state_size, out_changed);
}

int smc_state_indexed_clear(smc_context_t *ctx) {
    if (!ctx || !ctx->initialized) {
        smc_set_error(SMC_ERR_INIT, "invalid context");
        return SMC_ERR_INIT;
    }
    if (!ctx->indexed_state_configured) {
        smc_set_error(SMC_ERR_INIT, "indexed state tracker not configured");
        return SMC_ERR_INIT;
    }
    return smc_indexed_state_table_clear(&ctx->indexed_state_table);
}

int smc_state_indexed_get_stats(smc_context_t *ctx, smc_state_indexed_stats_t *out) {
    if (!ctx || !ctx->initialized) {
        smc_set_error(SMC_ERR_INIT, "invalid context");
        return SMC_ERR_INIT;
    }
    if (!out) {
        smc_set_error(SMC_ERR_INVALID, "null stats output");
        return SMC_ERR_INVALID;
    }
    *out = ctx->indexed_state_stats;
    return SMC_OK;
}

int smc_state_indexed_reset_stats(smc_context_t *ctx) {
    if (!ctx || !ctx->initialized) {
        smc_set_error(SMC_ERR_INIT, "invalid context");
        return SMC_ERR_INIT;
    }
    memset(&ctx->indexed_state_stats, 0, sizeof(ctx->indexed_state_stats));
    return SMC_OK;
}

int smc_state_diff_indexed_batch(smc_context_t *ctx,
                                    const void *states,
                                    size_t count,
                                    size_t stride,
                                    uint32_t *dirty_indices,
                                    size_t dirty_capacity,
                                    size_t *out_dirty_count) {
    if (!ctx || !ctx->initialized) {
        smc_set_error(SMC_ERR_INIT, "invalid context");
        return SMC_ERR_INIT;
    }
    if (!ctx->indexed_state_configured) {
        smc_set_error(SMC_ERR_INIT, "indexed state tracker not configured");
        return SMC_ERR_INIT;
    }
    return smc_indexed_state_table_diff_batch(&ctx->indexed_state_table, states,
                                                count, stride, dirty_indices,
                                                dirty_capacity, out_dirty_count);
}

int smc_state_diff_indexed_streams(smc_context_t *ctx,
                                      const smc_state_stream_t *streams,
                                      size_t stream_count,
                                      size_t record_count,
                                      uint32_t *dirty_indices,
                                      size_t dirty_capacity,
                                      size_t *out_dirty_count) {
    if (!ctx || !ctx->initialized) {
        smc_set_error(SMC_ERR_INIT, "invalid context");
        return SMC_ERR_INIT;
    }
    if (!ctx->indexed_state_configured) {
        smc_set_error(SMC_ERR_INIT, "indexed state tracker not configured");
        return SMC_ERR_INIT;
    }
    return smc_indexed_state_table_diff_streams(&ctx->indexed_state_table, streams,
                                                  stream_count, record_count,
                                                  dirty_indices, dirty_capacity,
                                                  out_dirty_count);
}

/* -------------------------------------------------------------------------- */
/* Introspection                                                              */
/* -------------------------------------------------------------------------- */

int smc_abi_version(void) {
    return SMC_ABI_VERSION;
}

const char *smc_runtime_kind(void) {
    return "stub";
}

uint32_t smc_features(void) {
    return SMC_FEATURE_ARTIFACT_CACHE | SMC_FEATURE_STATE_TRACKING
           | SMC_FEATURE_INDEXED_STATE_TRACKING;
}

/* -------------------------------------------------------------------------- */
/* Error handling                                                             */
/* -------------------------------------------------------------------------- */

const char *smc_error_string(int code) {
    switch (code) {
        case SMC_OK:            return "success";
        case SMC_ERR_INIT:      return "initialization error";
        case SMC_ERR_PARSE:     return "parse error";
        case SMC_ERR_EVAL:      return "evaluation error";
        case SMC_ERR_NOT_IMPL:  return "not implemented";
        case SMC_ERR_IO:        return "I/O error";
        case SMC_ERR_INVALID:   return "invalid argument";
        case SMC_ERR_ABI:       return "ABI version mismatch";
        case SMC_ERR_ARITY:     return "wrong arity";
        case SMC_ERR_NOT_FOUND: return "expression not found";
        case SMC_ERR_THREAD:    return "thread-safety violation";
        case SMC_ERR_SHUTDOWN:  return "library shut down";
        case SMC_ERR_SIZE:      return "size limit exceeded";
        case SMC_ERR_CAPACITY:  return "memory budget exceeded";
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

int smc_get_stats(smc_stats_t *out) {
    if (!out) {
        smc_set_error(SMC_ERR_INVALID, "null argument");
        return SMC_ERR_INVALID;
    }
    *out = *g_stats;
    return SMC_OK;
}

int smc_reset_stats(void) {
    memset(g_stats, 0, sizeof(*g_stats));
    return SMC_OK;
}