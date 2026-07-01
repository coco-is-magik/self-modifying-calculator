/* smc_runtime_stub.c — standalone C runtime for the Self-Modifying Calculator */
/*
 * This is the default Milestone 1 runtime. It implements the stable C API v1
 * without any dependency on SBCL. It contains a tiny recursive-descent parser
 * and evaluator for scalar arithmetic expressions.
 *
 * The stub is sufficient for demos, tests, and host-language bindings. For full
 * SMC features (hierarchical cache, runtime specialization, source rewriting),
 * build against smc_runtime_sbcl.c instead.
 */

#include "smc.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/* Error codes                                                                */
/* -------------------------------------------------------------------------- */

#define SMC_OK             0
#define SMC_ERR_INIT      -1
#define SMC_ERR_PARSE     -2
#define SMC_ERR_EVAL      -3
#define SMC_ERR_NOT_IMPL  -4
#define SMC_ERR_IO        -5
#define SMC_ERR_INVALID   -6

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

static void smc_set_errorf(int code, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    g_last_error.code = code;
    vsnprintf(g_last_error.message, sizeof(g_last_error.message), fmt, ap);
    g_last_error.message[sizeof(g_last_error.message) - 1] = '\0';
    va_end(ap);
}

/* -------------------------------------------------------------------------- */
/* Context                                                                    */
/* -------------------------------------------------------------------------- */

struct smc_context {
    int   level;
    int   initialized;
};

static smc_context_t g_global_context = {1, 0};
static int           g_initialized    = 0;

/* -------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

int smc_init(void) {
    if (g_initialized) {
        return SMC_OK;
    }
    g_initialized = 1;
    g_global_context.level = 1;
    g_global_context.initialized = 1;
    smc_set_error(SMC_OK, NULL);
    return SMC_OK;
}

int smc_shutdown(void) {
    g_initialized = 0;
    g_global_context.initialized = 0;
    return SMC_OK;
}

smc_context_t *smc_context_create(int level) {
    smc_context_t *ctx = (smc_context_t *)malloc(sizeof(smc_context_t));
    if (!ctx) {
        smc_set_error(SMC_ERR_INIT, "failed to allocate context");
        return NULL;
    }
    ctx->level = (level < 1) ? 1 : (level > 3 ? 3 : level);
    ctx->initialized = 1;
    return ctx;
}

void smc_context_destroy(smc_context_t *ctx) {
    if (ctx) {
        ctx->initialized = 0;
        free(ctx);
    }
}

/* -------------------------------------------------------------------------- */
/* Global context convenience API                                             */
/* -------------------------------------------------------------------------- */

int smc_set_optimization_level(int level) {
    if (!g_initialized) {
        smc_set_error(SMC_ERR_INIT, "library not initialized");
        return SMC_ERR_INIT;
    }
    g_global_context.level = (level < 1) ? 1 : (level > 3 ? 3 : level);
    return SMC_OK;
}

int smc_get_optimization_level(void) {
    if (!g_initialized) {
        return 0;
    }
    return g_global_context.level;
}

/* -------------------------------------------------------------------------- */
/* Tiny expression parser / evaluator                                         */
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

static int smc_parse_expression(smc_parser_t *p, double *out);

static int smc_parse_number(smc_parser_t *p, double *out) {
    smc_parser_skip_ws(p);
    size_t start = p->pos;
    int has_dot = 0;

    while (p->pos < p->len) {
        char c = p->s[p->pos];
        if (c == '.') {
            if (has_dot) {
                break;
            }
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

static int smc_parse_primary(smc_parser_t *p, double *out) {
    smc_parser_skip_ws(p);
    int c = smc_parser_peek(p);
    if (c < 0) {
        smc_set_error(SMC_ERR_PARSE, "unexpected end of expression");
        return SMC_ERR_PARSE;
    }

    if (c == '(') {
        smc_parser_get(p);
        int rc = smc_parse_expression(p, out);
        if (rc != SMC_OK) {
            return rc;
        }
        return smc_parser_expect(p, ')');
    }

    if (isdigit((unsigned char)c) || c == '.') {
        return smc_parse_number(p, out);
    }

    smc_set_errorf(SMC_ERR_PARSE, "unexpected character '%c' at position %zu", c, p->pos);
    return SMC_ERR_PARSE;
}

static int smc_parse_power(smc_parser_t *p, double *out) {
    int rc = smc_parse_primary(p, out);
    if (rc != SMC_OK) {
        return rc;
    }
    if (smc_parser_peek(p) == '^') {
        smc_parser_get(p);
        double rhs;
        /* Right-associative: a^b^c == a^(b^c) */
        rc = smc_parse_power(p, &rhs);
        if (rc != SMC_OK) {
            return rc;
        }
        *out = pow(*out, rhs);
    }
    return SMC_OK;
}

static int smc_parse_unary(smc_parser_t *p, double *out) {
    int c = smc_parser_peek(p);
    if (c == '+' || c == '-') {
        smc_parser_get(p);
        int rc = smc_parse_unary(p, out);
        if (rc != SMC_OK) {
            return rc;
        }
        if (c == '-') {
            *out = -(*out);
        }
        return SMC_OK;
    }
    return smc_parse_power(p, out);
}

static int smc_parse_term(smc_parser_t *p, double *out) {
    int rc = smc_parse_unary(p, out);
    if (rc != SMC_OK) {
        return rc;
    }
    for (;;) {
        int c = smc_parser_peek(p);
        if (c != '*' && c != '/') {
            break;
        }
        smc_parser_get(p);
        double rhs;
        rc = smc_parse_unary(p, &rhs);
        if (rc != SMC_OK) {
            return rc;
        }
        if (c == '*') {
            *out *= rhs;
        } else {
            if (rhs == 0.0) {
                smc_set_error(SMC_ERR_EVAL, "division by zero");
                return SMC_ERR_EVAL;
            }
            *out /= rhs;
        }
    }
    return SMC_OK;
}

static int smc_parse_expression(smc_parser_t *p, double *out) {
    int rc = smc_parse_term(p, out);
    if (rc != SMC_OK) {
        return rc;
    }
    for (;;) {
        int c = smc_parser_peek(p);
        if (c != '+' && c != '-') {
            break;
        }
        smc_parser_get(p);
        double rhs;
        rc = smc_parse_term(p, &rhs);
        if (rc != SMC_OK) {
            return rc;
        }
        if (c == '+') {
            *out += rhs;
        } else {
            *out -= rhs;
        }
    }
    return SMC_OK;
}

/* -------------------------------------------------------------------------- */
/* Tier 1 evaluation                                                          */
/* -------------------------------------------------------------------------- */

static int smc_eval_double_impl(smc_context_t *ctx, const char *expr, double *out) {
    (void)ctx;
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
    int rc = smc_parse_expression(&p, out);
    if (rc != SMC_OK) {
        return rc;
    }
    if (smc_parser_peek(&p) >= 0) {
        smc_set_error(SMC_ERR_PARSE, "trailing characters in expression");
        return SMC_ERR_PARSE;
    }
    return SMC_OK;
}

int smc_eval_double(const char *expr, double *out) {
    return smc_eval_double_impl(&g_global_context, expr, out);
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
    if (rc == SMC_OK && out) {
        *out = (float)d;
    }
    return rc;
}

int smc_eval_float_with(smc_context_t *ctx, const char *expr, float *out) {
    double d;
    int rc = smc_eval_double_with(ctx, expr, &d);
    if (rc == SMC_OK && out) {
        *out = (float)d;
    }
    return rc;
}

int smc_eval_int(const char *expr, int64_t *out) {
    double d;
    int rc = smc_eval_double(expr, &d);
    if (rc == SMC_OK && out) {
        *out = (int64_t)d;
    }
    return rc;
}

int smc_eval_int_with(smc_context_t *ctx, const char *expr, int64_t *out) {
    double d;
    int rc = smc_eval_double_with(ctx, expr, &d);
    if (rc == SMC_OK && out) {
        *out = (int64_t)d;
    }
    return rc;
}

/* -------------------------------------------------------------------------- */
/* Tier 2 generated-code calls                                                */
/* -------------------------------------------------------------------------- */
/*
 * The actual implementations of smc_call_* and smc_expr_* are supplied by the
 * generated dispatch table (smc_generated.c).  The smc_generated_runtime.c
 * companion file provides weak fallbacks that return SMC_ERR_NOT_IMPL when no
 * generated table is linked.
 */

/* -------------------------------------------------------------------------- */
/* Variables (stub: not implemented in Milestone 1)                             */
/* -------------------------------------------------------------------------- */

int smc_set_variable_double(const char *name, double value) {
    (void)name;
    (void)value;
    smc_set_error(SMC_ERR_NOT_IMPL, "variables not implemented in stub runtime");
    return SMC_ERR_NOT_IMPL;
}

int smc_set_variable_double_with(smc_context_t *ctx, const char *name, double value) {
    (void)ctx;
    (void)name;
    (void)value;
    smc_set_error(SMC_ERR_NOT_IMPL, "variables not implemented in stub runtime");
    return SMC_ERR_NOT_IMPL;
}

int smc_clear_variables(void) {
    smc_set_error(SMC_ERR_NOT_IMPL, "variables not implemented in stub runtime");
    return SMC_ERR_NOT_IMPL;
}

int smc_clear_variables_with(smc_context_t *ctx) {
    (void)ctx;
    smc_set_error(SMC_ERR_NOT_IMPL, "variables not implemented in stub runtime");
    return SMC_ERR_NOT_IMPL;
}

/* -------------------------------------------------------------------------- */
/* Cache control (stub: not implemented in Milestone 1)                       */
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
    (void)ctx;
    (void)path;
    smc_set_error(SMC_ERR_NOT_IMPL, "cache persistence not implemented in stub runtime");
    return SMC_ERR_NOT_IMPL;
}

int smc_cache_load(const char *path) {
    (void)path;
    smc_set_error(SMC_ERR_NOT_IMPL, "cache persistence not implemented in stub runtime");
    return SMC_ERR_NOT_IMPL;
}

int smc_cache_load_with(smc_context_t *ctx, const char *path) {
    (void)ctx;
    (void)path;
    smc_set_error(SMC_ERR_NOT_IMPL, "cache persistence not implemented in stub runtime");
    return SMC_ERR_NOT_IMPL;
}

/* -------------------------------------------------------------------------- */
/* Source generation (stub: not implemented in Milestone 1)                   */
/* -------------------------------------------------------------------------- */

int smc_generate_c_source(const char *out_path) {
    (void)out_path;
    smc_set_error(SMC_ERR_NOT_IMPL, "source generation not implemented in stub runtime");
    return SMC_ERR_NOT_IMPL;
}

int smc_generate_c_source_with(smc_context_t *ctx, const char *out_path) {
    (void)ctx;
    (void)out_path;
    smc_set_error(SMC_ERR_NOT_IMPL, "source generation not implemented in stub runtime");
    return SMC_ERR_NOT_IMPL;
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
