#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static volatile double global_sink = 0.0;

struct options {
    size_t n;
    size_t tile_size;
    size_t passes;
};

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <n> <tile_size> [passes]\n", prog);
}

static size_t parse_size_value(const char *s) {
    char *end = NULL;
    unsigned long long value;

    errno = 0;
    value = strtoull(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || value == 0 || value > SIZE_MAX) {
        return 0;
    }
    return (size_t)value;
}

static int parse_args(int argc, char **argv, struct options *opts) {
    opts->n = 0;
    opts->tile_size = 0;
    opts->passes = 1;

    if (argc < 3 || argc > 4) {
        return -1;
    }

    opts->n = parse_size_value(argv[1]);
    opts->tile_size = parse_size_value(argv[2]);
    if (opts->n == 0 || opts->tile_size == 0) {
        return -1;
    }
    if (argc == 4) {
        opts->passes = parse_size_value(argv[3]);
        if (opts->passes == 0) {
            return -1;
        }
    }

    return 0;
}

static double diff_seconds(const struct timespec *start,
                           const struct timespec *end) {
    time_t sec = end->tv_sec - start->tv_sec;
    long nsec = end->tv_nsec - start->tv_nsec;

    if (nsec < 0) {
        sec -= 1;
        nsec += 1000000000L;
    }

    return (double)sec + (double)nsec / 1e9;
}

static inline double *cell(double *base, size_t n, size_t i, size_t j) {
    return &base[i * n + j];
}

static void fill_matrix(double *matrix, size_t n, double scale) {
    size_t i;
    size_t total = n * n;

    for (i = 0; i < total; i++) {
        matrix[i] = (double)((i % 1024U) + 1U) * scale;
    }
}

/*
 * Tiled matrix multiply benchmark.
 *
 * Goal:
 * - Measure how blocking changes reuse when multiplying row-major matrices.
 * - Tile size controls the reuse window; smaller tiles can improve cache
 *   locality until loop overhead starts to dominate.
 *
 * The benchmark keeps a single tiled kernel so the measurement remains focused
 * on blocking behavior rather than comparing many algorithm variants.
 */
static void matmul_tiled(double *a, double *b, double *c, size_t n, size_t tile) {
    size_t ii;
    size_t jj;
    size_t kk;
    size_t i;
    size_t j;
    size_t k;

    for (ii = 0; ii < n; ii += tile) {
        size_t i_end = ii + tile < n ? ii + tile : n;
        for (kk = 0; kk < n; kk += tile) {
            size_t k_end = kk + tile < n ? kk + tile : n;
            for (jj = 0; jj < n; jj += tile) {
                size_t j_end = jj + tile < n ? jj + tile : n;
                for (i = ii; i < i_end; i++) {
                    for (k = kk; k < k_end; k++) {
                        double a_ik = *cell(a, n, i, k);
                        for (j = jj; j < j_end; j++) {
                            *cell(c, n, i, j) += a_ik * *cell(b, n, k, j);
                        }
                    }
                }
            }
        }
    }
}

static double checksum_matrix(const double *matrix, size_t n) {
    size_t i;
    size_t total = n * n;
    double checksum = 0.0;

    for (i = 0; i < total; i++) {
        checksum += matrix[i];
    }
    return checksum;
}

int main(int argc, char **argv) {
    struct options opts;
    struct timespec start_ts;
    struct timespec end_ts;
    double *a = NULL;
    double *b = NULL;
    double *c = NULL;
    size_t total_elems;
    size_t bytes;
    double elapsed_seconds;
    double checksum;
    double total_flops;
    double gflops;
    size_t pass;

    if (parse_args(argc, argv, &opts) != 0) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (opts.n > SIZE_MAX / opts.n || opts.n * opts.n > SIZE_MAX / sizeof(double)) {
        fprintf(stderr, "matrix size is too large\n");
        return EXIT_FAILURE;
    }

    total_elems = opts.n * opts.n;
    if (total_elems > SIZE_MAX / sizeof(double) ||
        total_elems * sizeof(double) > SIZE_MAX / 3) {
        fprintf(stderr, "matrix size is too large for output accounting\n");
        return EXIT_FAILURE;
    }

    if (opts.tile_size > opts.n) {
        opts.tile_size = opts.n;
    }

    bytes = total_elems * sizeof(double);
    if (posix_memalign((void **)&a, 64, bytes) != 0 ||
        posix_memalign((void **)&b, 64, bytes) != 0 ||
        posix_memalign((void **)&c, 64, bytes) != 0) {
        perror("posix_memalign");
        free(a);
        free(b);
        free(c);
        return EXIT_FAILURE;
    }

    fill_matrix(a, opts.n, 0.5);
    fill_matrix(b, opts.n, 0.25);
    memset(c, 0, bytes);

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &start_ts) != 0) {
        perror("clock_gettime(start)");
        free(a);
        free(b);
        free(c);
        return EXIT_FAILURE;
    }

    for (pass = 0; pass < opts.passes; pass++) {
        matmul_tiled(a, b, c, opts.n, opts.tile_size);
    }

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &end_ts) != 0) {
        perror("clock_gettime(end)");
        free(a);
        free(b);
        free(c);
        return EXIT_FAILURE;
    }

    checksum = checksum_matrix(c, opts.n);
    global_sink = checksum;
    __asm__ volatile("" : : "m"(global_sink) : "memory");

    elapsed_seconds = diff_seconds(&start_ts, &end_ts);
    total_flops = 2.0 * (double)opts.n * (double)opts.n * (double)opts.n * (double)opts.passes;
    gflops = total_flops / elapsed_seconds / 1e9;

    printf("matmul_tiled,tiled,%zu,%zu,%zu,%zu,%d,%zu,%.9f,%.6f\n",
           3 * bytes,
           total_elems,
           opts.n,
           opts.n,
           0,
           opts.tile_size,
           elapsed_seconds,
           gflops);

    free(a);
    free(b);
    free(c);
    return EXIT_SUCCESS;
}
