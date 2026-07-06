/* tests/c/test_thread_safety.c — exercise the global context mutex under SMC_THREAD_SAFE.
 *
 * This test is only built when SMC_THREAD_SAFE=ON. It spawns several worker
 * threads that concurrently use Tier 1 evaluation and per-thread contexts, and
 * verifies that the results are correct and that the process does not crash.
 */

#include "smc.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_THREADS 4
#define ITERATIONS_PER_THREAD 1000

static int g_failed = 0;
static pthread_mutex_t g_failed_mutex = PTHREAD_MUTEX_INITIALIZER;

static void record_failure(const char *msg) {
    pthread_mutex_lock(&g_failed_mutex);
    if (!g_failed) {
        fprintf(stderr, "FAIL: %s\n", msg);
    }
    g_failed = 1;
    pthread_mutex_unlock(&g_failed_mutex);
}

static void *worker_global_context(void *arg) {
    (void)arg;
    for (int i = 0; i < ITERATIONS_PER_THREAD; i++) {
        double out = 0.0;
        char expr[64];
        snprintf(expr, sizeof(expr), "%d + %d", i, i);
        int rc = smc_eval_double(expr, &out);
        if (rc != SMC_OK) {
            record_failure("global context eval failed");
            return NULL;
        }
        if (out != (double)(i + i)) {
            record_failure("global context eval produced wrong result");
            return NULL;
        }
    }
    return NULL;
}

static void *worker_per_thread_context(void *arg) {
    (void)arg;
    smc_context_t *ctx = smc_context_create(1);
    if (!ctx) {
        record_failure("failed to create per-thread context");
        return NULL;
    }

    for (int i = 0; i < ITERATIONS_PER_THREAD; i++) {
        double out = 0.0;
        char expr[64];
        snprintf(expr, sizeof(expr), "%d * %d", i, 2);
        int rc = smc_eval_double_with(ctx, expr, &out);
        if (rc != SMC_OK) {
            record_failure("per-thread context eval failed");
            smc_context_destroy(ctx);
            return NULL;
        }
        if (out != (double)(i * 2)) {
            record_failure("per-thread context eval produced wrong result");
            smc_context_destroy(ctx);
            return NULL;
        }
    }

    smc_context_destroy(ctx);
    return NULL;
}

static void *worker_generated_hot_path(void *arg) {
    (void)arg;
    int count = smc_expr_count();
    if (count <= 0) {
        /* Nothing to do if no generated table is linked. */
        return NULL;
    }

    for (int i = 0; i < ITERATIONS_PER_THREAD; i++) {
        /* Cycle through all generated IDs. smc_call_* is stateless/read-only. */
        smc_expr_id_t id = (smc_expr_id_t)((i % count) + 1);
        size_t arity = smc_expr_arity(id);
        double args[2] = {0.0, 0.0};
        double out = 0.0;
        int rc = smc_call_double(id, arity == 0 ? NULL : args, arity, &out);
        if (rc != SMC_OK && rc != SMC_ERR_NOT_FOUND) {
            record_failure("generated hot-path call failed unexpectedly");
            return NULL;
        }
    }
    return NULL;
}

int main(void) {
    int rc = smc_init();
    if (rc != SMC_OK) {
        fprintf(stderr, "smc_init failed: %d\n", rc);
        return 1;
    }

    pthread_t threads[NUM_THREADS * 3];

    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, worker_global_context, NULL) != 0) {
            record_failure("pthread_create failed");
            goto cleanup;
        }
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[NUM_THREADS + i], NULL, worker_per_thread_context, NULL) != 0) {
            record_failure("pthread_create failed");
            goto cleanup;
        }
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[2 * NUM_THREADS + i], NULL, worker_generated_hot_path, NULL) != 0) {
            record_failure("pthread_create failed");
            goto cleanup;
        }
    }

    for (int i = 0; i < NUM_THREADS * 3; i++) {
        pthread_join(threads[i], NULL);
    }

cleanup:
    smc_shutdown();

    if (g_failed) {
        return 1;
    }

    printf("All thread-safety tests passed.\n");
    return 0;
}
