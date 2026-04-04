#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static volatile double global_sink = 0.0;

enum variant {
    VARIANT_NAIVE,
    VARIANT_REORDERED,
    VARIANT_BLOCKED
};

struct options {
    enum variant variant;
    size_t n;
    size_t block_size;
};

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s --variant <naive|reordered|blocked> --n <size> [--block <size>]\n",
            prog);
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

static int parse_variant(const char *s, enum variant *variant) {
    if (strcmp(s, "naive") == 0) {
        *variant = VARIANT_NAIVE;
        return 0;
    }
    if (strcmp(s, "reordered") == 0) {
        *variant = VARIANT_REORDERED;
        return 0;
    }
    if (strcmp(s, "blocked") == 0) {
        *variant = VARIANT_BLOCKED;
        return 0;
    }
    return -1;
}

static int parse_args(int argc, char **argv, struct options *opts) {
    int i;

    opts->variant = VARIANT_NAIVE;
    opts->n = 0;
    opts->block_size = 32;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--variant") == 0 && i + 1 < argc) {
            if (parse_variant(argv[++i], &opts->variant) != 0) {
                return -1;
            }
        } else if (strcmp(argv[i], "--n") == 0 && i + 1 < argc) {
            opts->n = parse_size_value(argv[++i]);
            if (opts->n == 0) {
                return -1;
            }
        } else if (strcmp(argv[i], "--block") == 0 && i + 1 < argc) {
            opts->block_size = parse_size_value(argv[++i]);
            if (opts->block_size == 0) {
                return -1;
            }
        } else {
            return -1;
        }
    }

    if (opts->n == 0) {
        return -1;
    }
    if (opts->variant != VARIANT_BLOCKED) {
        opts->block_size = 0;
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

static void matmul_naive(double *a, double *b, double *c, size_t n) {
    size_t i;
    size_t j;
    size_t k;

    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            double sum = 0.0;
            for (k = 0; k < n; k++) {
                sum += *cell(a, n, i, k) * *cell(b, n, k, j);
            }
            *cell(c, n, i, j) = sum;
        }
    }
}

static void matmul_reordered(double *a, double *b, double *c, size_t n) {
    size_t i;
    size_t j;
    size_t k;

    for (i = 0; i < n; i++) {
        for (k = 0; k < n; k++) {
            double a_ik = *cell(a, n, i, k);
            for (j = 0; j < n; j++) {
                *cell(c, n, i, j) += a_ik * *cell(b, n, k, j);
            }
        }
    }
}

static void matmul_blocked(double *a, double *b, double *c, size_t n, size_t block) {
    size_t ii;
    size_t jj;
    size_t kk;
    size_t i;
    size_t j;
    size_t k;

    for (ii = 0; ii < n; ii += block) {
        size_t i_end = ii + block < n ? ii + block : n;
        for (kk = 0; kk < n; kk += block) {
            size_t k_end = kk + block < n ? kk + block : n;
            for (jj = 0; jj < n; jj += block) {
                size_t j_end = jj + block < n ? jj + block : n;
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
    size_t bytes;
    double elapsed_seconds;
    double checksum;

    if (parse_args(argc, argv, &opts) != 0) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (opts.n > SIZE_MAX / opts.n || opts.n * opts.n > SIZE_MAX / sizeof(double)) {
        fprintf(stderr, "matrix size is too large\n");
        return EXIT_FAILURE;
    }

    bytes = opts.n * opts.n * sizeof(double);
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

    switch (opts.variant) {
    case VARIANT_NAIVE:
        matmul_naive(a, b, c, opts.n);
        break;
    case VARIANT_REORDERED:
        matmul_reordered(a, b, c, opts.n);
        break;
    case VARIANT_BLOCKED:
        matmul_blocked(a, b, c, opts.n, opts.block_size);
        break;
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

    printf("benchmark=matrix\n");
    printf("variant=%s\n",
           opts.variant == VARIANT_NAIVE ? "naive" :
           (opts.variant == VARIANT_REORDERED ? "reordered" : "blocked"));
    printf("n=%zu\n", opts.n);
    printf("block_size=%zu\n", opts.block_size);
    printf("elapsed_seconds=%.9f\n", elapsed_seconds);
    printf("checksum=%.17g\n", checksum);

    free(a);
    free(b);
    free(c);
    return EXIT_SUCCESS;
}
