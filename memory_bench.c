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

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s <seq|stride|rand> <array_size> <stride>\n"
            "  <array_size>: bytes, optionally using K/M/G suffixes (e.g. 1048576, 64M, 1G)\n"
            "  <stride>:     element stride for stride mode; ignored for seq/rand\n",
            prog);
}

static int parse_access_mode(const char *s, enum access_mode *mode) {
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

static size_t parse_size_bytes(const char *s) {
    char *end = NULL;
    errno = 0;
    unsigned long long value = strtoull(s, &end, 10);
    if (errno != 0 || end == s) {
        return 0;
    }

    unsigned long long mult = 1;
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

static double timespec_diff_seconds(const struct timespec *start,
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
        size_t j = (size_t)(seed % (i + 1));
        size_t tmp = idx[i];
        idx[i] = idx[j];
        idx[j] = tmp;
    }
}

static size_t compute_repetitions(size_t elements, size_t touches_per_pass) {
    const size_t target_touches = 1ULL << 28;
    if (touches_per_pass == 0) {
        return 1;
    }

    size_t reps = target_touches / touches_per_pass;
    if (reps < 1) {
        reps = 1;
    }
    if (reps > (1U << 20)) {
        reps = (1U << 20);
    }

    (void)elements;
    return reps;
}

int main(int argc, char **argv) {
    enum access_mode mode;
    size_t array_size_bytes, elements, stride = 1;
    uint64_t *array = NULL;
    size_t *rand_idx = NULL;
    size_t i, rep, touches_per_pass, reps;
    uint64_t sum = 0;
    struct timespec start_ts, end_ts;
    double elapsed_sec;

    if (argc != 4) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (parse_access_mode(argv[1], &mode) != 0) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    array_size_bytes = parse_size_bytes(argv[2]);
    if (array_size_bytes < sizeof(uint64_t)) {
        fprintf(stderr, "array_size must be at least %zu bytes\n", sizeof(uint64_t));
        return EXIT_FAILURE;
    }

    if (mode == MODE_STRIDE) {
        stride = parse_size_bytes(argv[3]);
        if (stride == 0) {
            fprintf(stderr, "stride must be a positive integer number of elements\n");
            return EXIT_FAILURE;
        }
    }

    elements = array_size_bytes / sizeof(uint64_t);
    if (elements == 0) {
        fprintf(stderr, "array_size is too small after element conversion\n");
        return EXIT_FAILURE;
    }

    if (posix_memalign((void **)&array, 64, elements * sizeof(*array)) != 0) {
        perror("posix_memalign(array)");
        return EXIT_FAILURE;
    }

    for (i = 0; i < elements; i++) {
        array[i] = (uint64_t)i ^ 0x9e3779b97f4a7c15ULL;
    }

    if (mode == MODE_RAND) {
        rand_idx = malloc(elements * sizeof(*rand_idx));
        if (rand_idx == NULL) {
            perror("malloc(rand_idx)");
            free(array);
            return EXIT_FAILURE;
        }
        shuffle_indices(rand_idx, elements, 0xdeadbeefcafebabeULL);
    }

    switch (mode) {
    case MODE_SEQ:
        touches_per_pass = elements;
        break;
    case MODE_STRIDE:
        touches_per_pass = (elements + stride - 1) / stride;
        break;
    case MODE_RAND:
        touches_per_pass = elements;
        break;
    default:
        free(rand_idx);
        free(array);
        return EXIT_FAILURE;
    }

    reps = compute_repetitions(elements, touches_per_pass);

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &start_ts) != 0) {
        perror("clock_gettime(start)");
        free(rand_idx);
        free(array);
        return EXIT_FAILURE;
    }

    for (rep = 0; rep < reps; rep++) {
        switch (mode) {
        case MODE_SEQ:
            for (i = 0; i < elements; i++) {
                sum += array[i];
            }
            break;
        case MODE_STRIDE:
            for (i = 0; i < elements; i += stride) {
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

    elapsed_sec = timespec_diff_seconds(&start_ts, &end_ts);

    printf("mode=%s\n", argv[1]);
    printf("array_bytes=%zu\n", array_size_bytes);
    printf("elements=%zu\n", elements);
    printf("stride_elements=%zu\n", stride);
    printf("touches_per_pass=%zu\n", touches_per_pass);
    printf("repetitions=%zu\n", reps);
    printf("total_touches=%zu\n", touches_per_pass * reps);
    printf("elapsed_seconds=%.9f\n", elapsed_sec);
    printf("sink=%" PRIu64 "\n", global_sink);

    free(rand_idx);
    free(array);
    return EXIT_SUCCESS;
}
