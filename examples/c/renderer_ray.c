/* examples/c/renderer_ray.c — realistic renderer hot-path example.
 *
 * This example shows how a simple ray-tracing-style renderer can use SMC's
 * Tier 2 generated-code API for shading expressions. The expressions are
 * warmed in SBCL, generated to C, and then called by stable ID inside a
 * per-pixel loop.
 *
 * Build:
 *   sbcl --script scripts/generate-c-source.lisp build/smc_generated.c
 *   cmake -B build -S . -DSMC_GENERATED_SOURCE=build/smc_generated.c
 *   cmake --build build
 *   ./build/renderer_ray
 */

#define _POSIX_C_SOURCE 200809L

#include "smc.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* A tiny 2D "framebuffer" represented as a flat array of doubles. */
#define WIDTH 64
#define HEIGHT 64

static double framebuffer[WIDTH * HEIGHT];

/* Evaluate a generated expression by ID, returning a safe default on error. */
static double shade_pixel(smc_expr_id_t id, const double *args, size_t argc) {
    double out = 0.0;
    int rc = smc_call_double(id, args, argc, &out);
    if (rc != SMC_OK) {
        /* Safe default: neutral grey. */
        return 0.5;
    }
    return out;
}

int main(void) {
    if (smc_init() != SMC_OK) {
        fprintf(stderr, "smc_init failed\n");
        return 1;
    }

    int count = smc_expr_count();
    printf("Renderer ray example ready. %d generated expression(s) available.\n", count);

    if (count == 0) {
        printf("No generated expressions; nothing to render.\n");
        smc_shutdown();
        return 0;
    }

    /* Find the expression IDs we expect from the renderer corpus. The default
     * warm-cache includes x^2 + y and x^2 + 5*x + 6; we use those as stand-ins
     * for shading terms. In a real build the corpus would contain dot products
     * and Blinn-Phong terms. */
    smc_expr_id_t diffuse_id = 0;
    smc_expr_id_t specular_id = 0;
    for (int id = 1; id <= count; id++) {
        const char *source = smc_expr_source((smc_expr_id_t)id);
        if (!source) continue;
        if (smc_expr_arity((smc_expr_id_t)id) == 2 && diffuse_id == 0) {
            diffuse_id = (smc_expr_id_t)id;
        } else if (smc_expr_arity((smc_expr_id_t)id) == 1 && specular_id == 0) {
            specular_id = (smc_expr_id_t)id;
        }
    }

    if (diffuse_id == 0) {
        printf("No arity-2 expression found; using first generated expression.\n");
        diffuse_id = 1;
    }
    if (specular_id == 0) {
        printf("No arity-1 expression found; using first generated expression.\n");
        specular_id = 1;
    }

    printf("Using diffuse id=%u source=%s\n", (unsigned int)diffuse_id,
           smc_expr_source(diffuse_id));
    printf("Using specular id=%u source=%s\n", (unsigned int)specular_id,
           smc_expr_source(specular_id));

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            /* Normalized pixel coordinates as fake "light" and "view" inputs. */
            double u = (double)x / (double)WIDTH;
            double v = (double)y / (double)HEIGHT;

            double diffuse_args[2] = {u, v};
            double diffuse = shade_pixel(diffuse_id, diffuse_args, 2);

            double specular_args[1] = {u + v};
            double specular = shade_pixel(specular_id, specular_args, 1);

            /* Combine diffuse and specular, clamped to [0, 1]. */
            double intensity = diffuse * 0.7 + specular * 0.3;
            if (intensity < 0.0) intensity = 0.0;
            if (intensity > 1.0) intensity = 1.0;
            framebuffer[y * WIDTH + x] = intensity;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1e9;

    /* Compute a simple checksum to prevent the compiler from dropping the work. */
    double checksum = 0.0;
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        checksum += framebuffer[i];
    }

    printf("Rendered %dx%d framebuffer in %.3f s (checksum=%g)\n",
           WIDTH, HEIGHT, elapsed, checksum);

    /* Print observability counters. */
    smc_stats_t stats;
    if (smc_get_stats(&stats) == SMC_OK) {
        printf("Stats: total_calls=%lu generated_hits=%lu fallback_evals=%lu "
               "invalid_ids=%lu arity_errors=%lu invalid_calls=%lu\n",
               (unsigned long)stats.total_calls,
               (unsigned long)stats.generated_hits,
               (unsigned long)stats.fallback_evals,
               (unsigned long)stats.invalid_ids,
               (unsigned long)stats.arity_errors,
               (unsigned long)stats.invalid_calls);
    }

    smc_shutdown();
    return 0;
}
