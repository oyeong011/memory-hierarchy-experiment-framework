#include "bench_util.h"

#include <stdio.h>
#include <string.h>

/*
 * STREAM-lite bandwidth benchmark.
 *
 * Goal:
 * - Observe how sequential bandwidth changes with working-set size.
 * - Compare simple streaming kernels that benefit from spatial locality and
 *   memory-level parallelism.
 *
 * Variants:
 * - copy:  a[i] = b[i]
 * - add:   c[i] = a[i] + b[i]
 * - triad: a[i] = b[i] + scalar * c[i]
 *
 * CSV columns:
 * benchmark,variant,size_bytes,elements,rows,cols,stride,tile_size,elapsed_sec,extra
 *
 * "extra" stores effective GB/s based on bytes moved by the selected kernel.
 */

static volatile double sink_value = 0.0;

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <elements> <copy|add|triad> [iterations]\n", prog);
}

static int bytes_per_element(const char *variant) {
    if (strcmp(variant, "copy") == 0) {
        return 2 * (int)sizeof(double);
    }
    if (strcmp(variant, "add") == 0) {
        return 3 * (int)sizeof(double);
    }
    if (strcmp(variant, "triad") == 0) {
        return 3 * (int)sizeof(double);
    }
    return 0;
}

int main(int argc, char **argv) {
    size_t elements;
    size_t iterations;
    const char *variant;
    double *a;
    double *b;
    double *c;
    double scalar = 3.0;
    size_t i;
    double start;
    double end;
    double elapsed;
    double moved_bytes;
    double bandwidth_gbps;
    int bytes_each;

    if (argc < 3 || argc > 4) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    elements = parse_size_arg(argv[1], "elements");
    variant = argv[2];
    iterations = (argc == 4) ? parse_size_arg(argv[3], "iterations") : 32;
    bytes_each = bytes_per_element(variant);
    if (bytes_each == 0) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    a = xaligned_alloc(64, elements * sizeof(*a));
    b = xaligned_alloc(64, elements * sizeof(*b));
    c = xaligned_alloc(64, elements * sizeof(*c));

    for (i = 0; i < elements; ++i) {
        a[i] = 1.0;
        b[i] = 2.0 + (double)(i % 13);
        c[i] = 0.5 + (double)(i % 7);
    }

    if (strcmp(variant, "copy") == 0) {
        for (i = 0; i < elements; ++i) {
            a[i] = b[i];
        }
        start = bench_now_seconds();
        for (size_t it = 0; it < iterations; ++it) {
            for (i = 0; i < elements; ++i) {
                a[i] = b[i];
            }
        }
        end = bench_now_seconds();
    } else if (strcmp(variant, "add") == 0) {
        for (i = 0; i < elements; ++i) {
            c[i] = a[i] + b[i];
        }
        start = bench_now_seconds();
        for (size_t it = 0; it < iterations; ++it) {
            for (i = 0; i < elements; ++i) {
                c[i] = a[i] + b[i];
            }
        }
        end = bench_now_seconds();
    } else {
        for (i = 0; i < elements; ++i) {
            a[i] = b[i] + scalar * c[i];
        }
        start = bench_now_seconds();
        for (size_t it = 0; it < iterations; ++it) {
            for (i = 0; i < elements; ++i) {
                a[i] = b[i] + scalar * c[i];
            }
        }
        end = bench_now_seconds();
    }

    sink_value = a[elements / 2] + b[elements / 3] + c[elements / 4];
    elapsed = end - start;
    moved_bytes = (double)elements * (double)bytes_each * (double)iterations;
    bandwidth_gbps = moved_bytes / elapsed / 1e9;

    printf("stream_lite,%s,%zu,%zu,0,0,1,0,%.9f,%.6f\n",
           variant,
           3 * elements * sizeof(*a),
           elements,
           elapsed,
           bandwidth_gbps);

    free(a);
    free(b);
    free(c);
    return EXIT_SUCCESS;
}
