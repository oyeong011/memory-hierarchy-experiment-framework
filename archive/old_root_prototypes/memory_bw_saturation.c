#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

enum mode {
    MODE_READ,
    MODE_WRITE,
    MODE_READWRITE
};

struct options {
    int threads;
    size_t bytes_per_thread;
    size_t iterations;
    enum mode mode;
    int pin_threads;
};

struct thread_ctx {
    int tid;
    const struct options *opts;
    uint64_t *buffer;
    size_t elements;
    uint64_t checksum;
    double elapsed_sec;
};

static volatile uint64_t global_sink = 0;

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s [options]\n"
            "\n"
            "Options:\n"
            "  -t, --threads <n>       Number of worker threads (default: 1)\n"
            "  -s, --size <bytes>      Bytes per thread, supports K/M/G suffixes (default: 256M)\n"
            "  -i, --iterations <n>    Full-buffer passes per thread (default: 32)\n"
            "  -m, --mode <name>       read | write | readwrite (default: readwrite)\n"
            "  -p, --pin               Pin threads to CPUs when possible\n"
            "  -h, --help              Show this help text\n"
            "\n"
            "Examples:\n"
            "  %s --threads 8 --size 512M --iterations 16 --mode read\n"
            "  %s -t 16 -s 256M -i 32 -m readwrite --pin\n",
            prog, prog, prog);
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

static int parse_mode(const char *s, enum mode *mode) {
    if (strcmp(s, "read") == 0) {
        *mode = MODE_READ;
        return 0;
    }
    if (strcmp(s, "write") == 0) {
        *mode = MODE_WRITE;
        return 0;
    }
    if (strcmp(s, "readwrite") == 0) {
        *mode = MODE_READWRITE;
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

static int parse_args(int argc, char **argv, struct options *opts) {
    int i;

    opts->threads = 1;
    opts->bytes_per_thread = 256ULL * 1024ULL * 1024ULL;
    opts->iterations = 32;
    opts->mode = MODE_READWRITE;
    opts->pin_threads = 0;

    for (i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--threads") == 0) && i + 1 < argc) {
            opts->threads = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--size") == 0) && i + 1 < argc) {
            opts->bytes_per_thread = parse_size_bytes(argv[++i]);
        } else if ((strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--iterations") == 0) && i + 1 < argc) {
            opts->iterations = parse_size_bytes(argv[++i]);
        } else if ((strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--mode") == 0) && i + 1 < argc) {
            if (parse_mode(argv[++i], &opts->mode) != 0) {
                return -1;
            }
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--pin") == 0) {
            opts->pin_threads = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            exit(EXIT_SUCCESS);
        } else {
            return -1;
        }
    }

    if (opts->threads <= 0 || opts->bytes_per_thread < sizeof(uint64_t) ||
        opts->iterations == 0) {
        return -1;
    }

    opts->bytes_per_thread = (opts->bytes_per_thread / 64) * 64;
    if (opts->bytes_per_thread == 0) {
        return -1;
    }

    return 0;
}

static void maybe_pin_thread(int tid) {
#ifdef __linux__
    cpu_set_t cpuset;
    long cpu_count = sysconf(_SC_NPROCESSORS_ONLN);

    if (cpu_count <= 0) {
        return;
    }

    CPU_ZERO(&cpuset);
    CPU_SET((unsigned int)tid % (unsigned int)cpu_count, &cpuset);
    (void)pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
#else
    (void)tid;
#endif
}

static void *worker_main(void *arg) {
    struct thread_ctx *ctx = arg;
    const struct options *opts = ctx->opts;
    struct timespec start_ts;
    struct timespec end_ts;
    uint64_t checksum = 0;
    size_t iter;
    size_t i;

    if (opts->pin_threads) {
        maybe_pin_thread(ctx->tid);
    }

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &start_ts) != 0) {
        perror("clock_gettime(start)");
        return NULL;
    }

    for (iter = 0; iter < opts->iterations; iter++) {
        switch (opts->mode) {
        case MODE_READ:
            for (i = 0; i < ctx->elements; i++) {
                checksum += ctx->buffer[i];
            }
            break;
        case MODE_WRITE:
            for (i = 0; i < ctx->elements; i++) {
                ctx->buffer[i] = (uint64_t)i + iter + (uint64_t)ctx->tid;
            }
            break;
        case MODE_READWRITE:
            for (i = 0; i < ctx->elements; i++) {
                uint64_t value = ctx->buffer[i];
                value += (uint64_t)iter + 1ULL;
                ctx->buffer[i] = value;
                checksum += value;
            }
            break;
        }
    }

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &end_ts) != 0) {
        perror("clock_gettime(end)");
        return NULL;
    }

    ctx->checksum = checksum;
    ctx->elapsed_sec = diff_seconds(&start_ts, &end_ts);
    return NULL;
}

static double bytes_per_iteration(const struct options *opts) {
    switch (opts->mode) {
    case MODE_READ:
        return (double)opts->bytes_per_thread;
    case MODE_WRITE:
        return (double)opts->bytes_per_thread;
    case MODE_READWRITE:
        return (double)opts->bytes_per_thread * 2.0;
    }
    return 0.0;
}

int main(int argc, char **argv) {
    struct options opts;
    pthread_t *threads = NULL;
    struct thread_ctx *ctxs = NULL;
    double max_elapsed = 0.0;
    double total_bytes;
    double total_mb;
    double bandwidth_mb_s;
    uint64_t total_checksum = 0;
    int rc = EXIT_FAILURE;
    int i;

    if (parse_args(argc, argv, &opts) != 0) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    threads = calloc((size_t)opts.threads, sizeof(*threads));
    ctxs = calloc((size_t)opts.threads, sizeof(*ctxs));
    if (threads == NULL || ctxs == NULL) {
        perror("calloc");
        goto out;
    }

    for (i = 0; i < opts.threads; i++) {
        size_t elements = opts.bytes_per_thread / sizeof(uint64_t);

        ctxs[i].tid = i;
        ctxs[i].opts = &opts;
        ctxs[i].elements = elements;

        if (posix_memalign((void **)&ctxs[i].buffer, 64, opts.bytes_per_thread) != 0) {
            perror("posix_memalign");
            goto out;
        }

        for (size_t j = 0; j < elements; j++) {
            ctxs[i].buffer[j] = (uint64_t)j ^ (0x9e3779b97f4a7c15ULL + (uint64_t)i);
        }
    }

    for (i = 0; i < opts.threads; i++) {
        if (pthread_create(&threads[i], NULL, worker_main, &ctxs[i]) != 0) {
            perror("pthread_create");
            goto out;
        }
    }

    for (i = 0; i < opts.threads; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("pthread_join");
            goto out;
        }
        if (ctxs[i].elapsed_sec > max_elapsed) {
            max_elapsed = ctxs[i].elapsed_sec;
        }
        total_checksum += ctxs[i].checksum;
    }

    total_bytes = bytes_per_iteration(&opts) * (double)opts.iterations * (double)opts.threads;
    total_mb = total_bytes / (1024.0 * 1024.0);
    bandwidth_mb_s = total_mb / max_elapsed;

    printf("threads=%d\n", opts.threads);
    printf("bytes_per_thread=%zu\n", opts.bytes_per_thread);
    printf("iterations=%zu\n", opts.iterations);
    printf("mode=%s\n",
           opts.mode == MODE_READ ? "read" :
           opts.mode == MODE_WRITE ? "write" : "readwrite");
    printf("pin_threads=%d\n", opts.pin_threads);
    printf("elapsed_sec=%.6f\n", max_elapsed);
    printf("total_bytes=%.0f\n", total_bytes);
    printf("bandwidth_MB_s=%.2f\n", bandwidth_mb_s);
    printf("checksum=%" PRIu64 "\n", total_checksum);

    global_sink = total_checksum;
    rc = EXIT_SUCCESS;

out:
    if (ctxs != NULL) {
        for (i = 0; i < opts.threads; i++) {
            free(ctxs[i].buffer);
        }
    }
    free(ctxs);
    free(threads);
    return rc;
}
