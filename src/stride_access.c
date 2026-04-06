#include "bench_util.h"

#include <inttypes.h>
#include <stdio.h>

/*
 * Stride access benchmark.
 *
 * Goal:
 * - Show how larger strides reduce spatial locality and waste more of each
 *   cache line fetch.
 * - Observe miss-rate and time-per-access changes as stride grows.
 *
 * CSV columns:
 * benchmark,variant,size_bytes,elements,rows,cols,stride,tile_size,elapsed_sec,extra
 *
 * "extra" stores average nanoseconds per touched element.
 */

static volatile uint64_t sink_sum = 0;

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <elements> <stride> [passes]\n", prog);
}

int main(int argc, char **argv) {
    size_t elements;
    size_t stride;
    size_t passes;
    uint64_t *array;
    uint64_t sum = 0;
    size_t i;
    size_t pass;
    size_t touches_per_pass;
    size_t total_touches;
    double start;
    double end;
    double elapsed;
    double ns_per_access;

    if (argc < 3 || argc > 4) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    elements = parse_size_arg(argv[1], "elements");
    stride = parse_size_arg(argv[2], "stride");
    passes = (argc == 4) ? parse_size_arg(argv[3], "passes") : 64;
    if (stride == 0 || stride > elements) {
        fprintf(stderr, "stride must be between 1 and elements\n");
        return EXIT_FAILURE;
    }

    array = xaligned_alloc(64, elements * sizeof(*array));
    for (i = 0; i < elements; ++i) {
        array[i] = (uint64_t)(i * 17u + 11u);
    }

    for (i = 0; i < elements; i += stride) {
        sum += array[i];
    }

    start = bench_now_seconds();
    for (pass = 0; pass < passes; ++pass) {
        for (i = 0; i < elements; i += stride) {
            sum += array[i];
        }
    }
    end = bench_now_seconds();

    sink_sum = sum;
    touches_per_pass = (elements + stride - 1) / stride;
    total_touches = touches_per_pass * passes;
    elapsed = end - start;
    ns_per_access = elapsed * 1e9 / (double)total_touches;

    printf("stride_access,sum,%zu,%zu,0,0,%zu,0,%.9f,%.3f\n",
           elements * sizeof(*array),
           elements,
           stride,
           elapsed,
           ns_per_access);

    free(array);
    return EXIT_SUCCESS;
}
