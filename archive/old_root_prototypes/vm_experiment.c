#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

static volatile uint64_t global_sink = 0;

enum access_mode {
    MODE_SEQ = 0,
    MODE_RAND = 1
};

struct options {
    size_t bytes;
    size_t passes;
    size_t page_size;
    enum access_mode mode;
    uint64_t seed;
};

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s --size <bytes> --passes <n> --mode <seq|rand> [--seed <n>]\n"
            "\n"
            "Examples:\n"
            "  %s --size 256M --passes 4 --mode seq\n"
            "  %s --size 2G --passes 2 --mode rand --seed 1234\n",
            prog, prog, prog);
}

static size_t parse_size(const char *s) {
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

static uint64_t parse_u64(const char *s) {
    char *end = NULL;
    uint64_t value;

    errno = 0;
    value = strtoull(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0') {
        return 0;
    }
    return value;
}

static int parse_mode(const char *s, enum access_mode *mode) {
    if (strcmp(s, "seq") == 0) {
        *mode = MODE_SEQ;
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

static uint64_t lcg_next(uint64_t *state) {
    *state = (*state * 6364136223846793005ULL) + 1ULL;
    return *state;
}

static void shuffle_pages(size_t *pages, size_t page_count, uint64_t seed) {
    size_t i;

    for (i = 0; i < page_count; i++) {
        pages[i] = i;
    }

    if (page_count < 2) {
        return;
    }

    for (i = page_count - 1; i > 0; i--) {
        size_t j = (size_t)(lcg_next(&seed) % (i + 1));
        size_t tmp = pages[i];
        pages[i] = pages[j];
        pages[j] = tmp;
    }
}

static int parse_args(int argc, char **argv, struct options *opts) {
    int i;

    opts->bytes = 256ULL * 1024ULL * 1024ULL;
    opts->passes = 1;
    opts->page_size = (size_t)sysconf(_SC_PAGESIZE);
    opts->mode = MODE_SEQ;
    opts->seed = 1;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
            opts->bytes = parse_size(argv[++i]);
        } else if (strcmp(argv[i], "--passes") == 0 && i + 1 < argc) {
            opts->passes = parse_size(argv[++i]);
        } else if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            if (parse_mode(argv[++i], &opts->mode) != 0) {
                return -1;
            }
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            opts->seed = parse_u64(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            exit(EXIT_SUCCESS);
        } else {
            return -1;
        }
    }

    if (opts->bytes == 0 || opts->passes == 0 || opts->page_size == 0) {
        return -1;
    }

    if (opts->bytes < opts->page_size) {
        opts->bytes = opts->page_size;
    }

    opts->bytes = (opts->bytes / opts->page_size) * opts->page_size;
    return 0;
}

int main(int argc, char **argv) {
    struct options opts;
    struct rusage before_usage;
    struct rusage after_usage;
    struct timespec start_ts;
    struct timespec end_ts;
    uint8_t *region = NULL;
    size_t *page_order = NULL;
    size_t page_count;
    size_t pass;
    size_t i;
    uint64_t checksum = 0;
    double elapsed;
    long long minor_faults;
    long long major_faults;

    if (parse_args(argc, argv, &opts) != 0) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    page_count = opts.bytes / opts.page_size;

    if (posix_memalign((void **)&region, opts.page_size, opts.bytes) != 0) {
        perror("posix_memalign");
        return EXIT_FAILURE;
    }

    page_order = malloc(page_count * sizeof(*page_order));
    if (page_order == NULL) {
        perror("malloc(page_order)");
        free(region);
        return EXIT_FAILURE;
    }

    if (opts.mode == MODE_RAND) {
        shuffle_pages(page_order, page_count, opts.seed);
    } else {
        for (i = 0; i < page_count; i++) {
            page_order[i] = i;
        }
    }

    if (getrusage(RUSAGE_SELF, &before_usage) != 0) {
        perror("getrusage(before)");
        free(page_order);
        free(region);
        return EXIT_FAILURE;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &start_ts) != 0) {
        perror("clock_gettime(start)");
        free(page_order);
        free(region);
        return EXIT_FAILURE;
    }

    for (pass = 0; pass < opts.passes; pass++) {
        for (i = 0; i < page_count; i++) {
            size_t page_idx = page_order[i];
            size_t offset = page_idx * opts.page_size;

            region[offset] = (uint8_t)((region[offset] + pass + i) & 0xffU);
            checksum += region[offset];
        }
    }

    if (clock_gettime(CLOCK_MONOTONIC, &end_ts) != 0) {
        perror("clock_gettime(end)");
        free(page_order);
        free(region);
        return EXIT_FAILURE;
    }

    if (getrusage(RUSAGE_SELF, &after_usage) != 0) {
        perror("getrusage(after)");
        free(page_order);
        free(region);
        return EXIT_FAILURE;
    }

    elapsed = diff_seconds(&start_ts, &end_ts);
    minor_faults = after_usage.ru_minflt - before_usage.ru_minflt;
    major_faults = after_usage.ru_majflt - before_usage.ru_majflt;
    global_sink = checksum;

    printf("mode=%s\n", opts.mode == MODE_SEQ ? "seq" : "rand");
    printf("bytes=%zu\n", opts.bytes);
    printf("pages=%zu\n", page_count);
    printf("page_size=%zu\n", opts.page_size);
    printf("passes=%zu\n", opts.passes);
    printf("elapsed_sec=%.6f\n", elapsed);
    printf("minor_faults=%lld\n", minor_faults);
    printf("major_faults=%lld\n", major_faults);
    printf("checksum=%" PRIu64 "\n", checksum);

    free(page_order);
    free(region);
    return EXIT_SUCCESS;
}
