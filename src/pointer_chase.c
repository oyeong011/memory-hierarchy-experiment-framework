#include "bench_util.h"

#include <inttypes.h>
#include <stdio.h>

/*
 * Pointer chasing benchmark.
 *
 * Goal:
 * - Observe latency growth as the random working set expands beyond cache levels.
 * - Use a strict load dependency chain so the compiler and hardware cannot turn
 *   the loop into a simple streaming access pattern.
 *
 * CSV columns:
 * benchmark,variant,size_bytes,elements,rows,cols,stride,tile_size,elapsed_sec,extra
 *
 * "extra" stores average nanoseconds per dependent load.
 */

static volatile size_t sink_index = 0;

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <elements> [iterations]\n", prog);
}

static void build_random_cycle(size_t *next, size_t elements) {
    size_t i;
    size_t *perm = xaligned_alloc(64, elements * sizeof(*perm));
    uint64_t seed = 0x1234abcddcba4321ULL;

    for (i = 0; i < elements; ++i) {
        perm[i] = i;
    }

    for (i = elements - 1; i > 0; --i) {
        size_t j = (size_t)(xorshift64(&seed) % (i + 1));
        size_t tmp = perm[i];
        perm[i] = perm[j];
        perm[j] = tmp;
    }

    for (i = 0; i + 1 < elements; ++i) {
        next[perm[i]] = perm[i + 1];
    }
    next[perm[elements - 1]] = perm[0];

    free(perm);
}

int main(int argc, char **argv) {
    size_t elements;
    size_t iterations;
    size_t *next;
    size_t current = 0;
    size_t warmup_steps;
    size_t i;
    double start;
    double end;
    double elapsed;
    double ns_per_load;

    if (argc < 2 || argc > 3) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    elements = parse_size_arg(argv[1], "elements");
    iterations = (argc == 3) ? parse_size_arg(argv[2], "iterations") : elements * 32;
    if (elements < 2) {
        fprintf(stderr, "elements must be at least 2\n");
        return EXIT_FAILURE;
    }
    if (iterations < elements) {
        iterations = elements;
    }

    next = xaligned_alloc(64, elements * sizeof(*next));
    build_random_cycle(next, elements);

    warmup_steps = (elements < 1024) ? elements : 1024;
    for (i = 0; i < warmup_steps; ++i) {
        current = next[current];
    }

    start = bench_now_seconds();
    for (i = 0; i < iterations; ++i) {
        current = next[current];
    }
    end = bench_now_seconds();

    sink_index = current;
    elapsed = end - start;
    ns_per_load = elapsed * 1e9 / (double)iterations;

    printf("pointer_chase,chase,%zu,%zu,0,0,1,0,%.9f,%.3f\n",
           elements * sizeof(*next),
           elements,
           elapsed,
           ns_per_load);

    free(next);
    return EXIT_SUCCESS;
}
