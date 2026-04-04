#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static volatile uint64_t global_sink = 0;

enum access_mode {
    MODE_SEQ,
    MODE_STRIDE,
    MODE_RAND
};

struct options {
    enum access_mode mode;
    size_t array_bytes;
    size_t stride_elements;
    size_t passes;
    uint64_t seed;
};

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s --mode <seq|stride|rand> --size <bytes> [--stride <elements>] [--passes <n>] [--seed <n>]\n",
            prog);
}

static size_t parse_size_bytes(const char *s) {
    char *end = NULL;
    unsigned long long value;
    unsigned long long mult = 1;

    errno = 0;
    value = strtoull(s, &end, 10);
    if (errno != 0 || end == s) {
        return 0;
    }

    if (*end != '\0') {
        if (end[1] != '\0') {
            return 0;
        }
        switch (*end) {
        case 'k':
        case 'K':
            mult = 1024ULL;
            break;
        case 'm':
        case 'M':
            mult = 1024ULL * 1024ULL;
            break;
        case 'g':
        case 'G':
            mult = 1024ULL * 1024ULL * 1024ULL;
            break;
        default:
            return 0;
        }
    }

    if (value > SIZE_MAX / mult) {
        return 0;
    }
    return (size_t)(value * mult);
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

static uint64_t parse_u64(const char *s) {
    char *end = NULL;
    unsigned long long value;

    errno = 0;
    value = strtoull(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0') {
        return 0;
    }
    return (uint64_t)value;
}

static int parse_mode(const char *s, enum access_mode *mode) {
    if (strcmp(s, "seq") == 0) {
        *mode = MODE_SEQ;
        return 0;
    }
    if (strcmp(s, "stride") == 0) {
        *mode = MODE_STRIDE;
        return 0;
    }
    if (strcmp(s, "rand") == 0) {
        *mode = MODE_RAND;
        return 0;
    }
    return -1;
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

static void shuffle_indices(size_t *idx, size_t n, uint64_t seed) {
    size_t i;

    for (i = 0; i < n; i++) {
        idx[i] = i;
    }

    for (i = n - 1; i > 0; i--) {
        seed = seed * 6364136223846793005ULL + 1ULL;
        {
            size_t j = (size_t)(seed % (i + 1));
            size_t tmp = idx[i];
            idx[i] = idx[j];
            idx[j] = tmp;
        }
    }
}

static int parse_args(int argc, char **argv, struct options *opts) {
    int i;

    opts->mode = MODE_SEQ;
    opts->array_bytes = 0;
    opts->stride_elements = 1;
    opts->passes = 8;
    opts->seed = 0xdeadbeefcafebabeULL;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            if (parse_mode(argv[++i], &opts->mode) != 0) {
                return -1;
            }
        } else if (strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
            opts->array_bytes = parse_size_bytes(argv[++i]);
            if (opts->array_bytes == 0) {
                return -1;
            }
        } else if (strcmp(argv[i], "--stride") == 0 && i + 1 < argc) {
            opts->stride_elements = parse_size_value(argv[++i]);
            if (opts->stride_elements == 0) {
                return -1;
            }
        } else if (strcmp(argv[i], "--passes") == 0 && i + 1 < argc) {
            opts->passes = parse_size_value(argv[++i]);
            if (opts->passes == 0) {
                return -1;
            }
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            opts->seed = parse_u64(argv[++i]);
        } else {
            return -1;
        }
    }

    if (opts->array_bytes < sizeof(uint64_t)) {
        return -1;
    }
    if (opts->mode != MODE_STRIDE) {
        opts->stride_elements = 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    struct options opts;
    struct timespec start_ts;
    struct timespec end_ts;
    uint64_t *array = NULL;
    size_t *rand_idx = NULL;
    size_t elements;
    size_t pass;
    size_t i;
    size_t touches_per_pass;
    uint64_t sum = 0;
    double elapsed_seconds;

    if (parse_args(argc, argv, &opts) != 0) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    elements = opts.array_bytes / sizeof(uint64_t);
    if (elements == 0) {
        fprintf(stderr, "array too small after conversion to uint64_t elements\n");
        return EXIT_FAILURE;
    }

    if (posix_memalign((void **)&array, 64, elements * sizeof(*array)) != 0) {
        perror("posix_memalign(array)");
        return EXIT_FAILURE;
    }

    for (i = 0; i < elements; i++) {
        array[i] = ((uint64_t)i * 1315423911ULL) ^ 0x9e3779b97f4a7c15ULL;
    }

    if (opts.mode == MODE_RAND) {
        rand_idx = malloc(elements * sizeof(*rand_idx));
        if (rand_idx == NULL) {
            perror("malloc(rand_idx)");
            free(array);
            return EXIT_FAILURE;
        }
        shuffle_indices(rand_idx, elements, opts.seed);
    }

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &start_ts) != 0) {
        perror("clock_gettime(start)");
        free(rand_idx);
        free(array);
        return EXIT_FAILURE;
    }

    for (pass = 0; pass < opts.passes; pass++) {
        switch (opts.mode) {
        case MODE_SEQ:
            for (i = 0; i < elements; i++) {
                sum += array[i];
            }
            break;
        case MODE_STRIDE:
            for (i = 0; i < elements; i += opts.stride_elements) {
                sum += array[i];
            }
            break;
        case MODE_RAND:
            for (i = 0; i < elements; i++) {
                sum += array[rand_idx[i]];
            }
            break;
        }
    }

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &end_ts) != 0) {
        perror("clock_gettime(end)");
        free(rand_idx);
        free(array);
        return EXIT_FAILURE;
    }

    global_sink = sum;
    __asm__ volatile("" : : "r"(global_sink) : "memory");

    elapsed_seconds = diff_seconds(&start_ts, &end_ts);
    if (opts.mode == MODE_STRIDE) {
        touches_per_pass = (elements + opts.stride_elements - 1) / opts.stride_elements;
    } else {
        touches_per_pass = elements;
    }

    printf("benchmark=memory_access\n");
    printf("mode=%s\n",
           opts.mode == MODE_SEQ ? "seq" :
           (opts.mode == MODE_STRIDE ? "stride" : "rand"));
    printf("array_bytes=%zu\n", opts.array_bytes);
    printf("elements=%zu\n", elements);
    printf("stride_elements=%zu\n", opts.stride_elements);
    printf("passes=%zu\n", opts.passes);
    printf("touches_per_pass=%zu\n", touches_per_pass);
    printf("elapsed_seconds=%.9f\n", elapsed_seconds);
    printf("sink=%" PRIu64 "\n", global_sink);

    free(rand_idx);
    free(array);
    return EXIT_SUCCESS;
}
