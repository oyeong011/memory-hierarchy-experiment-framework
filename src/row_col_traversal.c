#include "bench_util.h"

#include <stdio.h>
#include <string.h>

/*
 * Row-major vs column-major traversal benchmark.
 *
 * Goal:
 * - Compare traversal orders over the same row-major matrix layout.
 * - Row traversal should preserve spatial locality. Column traversal should
 *   trigger more cache line waste and cache/TLB pressure on large matrices.
 *
 * CSV columns:
 * benchmark,variant,size_bytes,elements,rows,cols,stride,tile_size,elapsed_sec,extra
 *
 * "extra" stores average nanoseconds per visited element.
 */

static volatile double sink_total = 0.0;

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <rows> <cols> <row|col> [passes]\n", prog);
}

int main(int argc, char **argv) {
    size_t rows;
    size_t cols;
    const char *order;
    size_t passes;
    double *matrix;
    double total = 0.0;
    size_t r;
    size_t c;
    size_t pass;
    double start;
    double end;
    double elapsed;
    double ns_per_element;

    if (argc < 4 || argc > 5) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    rows = parse_size_arg(argv[1], "rows");
    cols = parse_size_arg(argv[2], "cols");
    order = argv[3];
    passes = (argc == 5) ? parse_size_arg(argv[4], "passes") : 16;
    if (strcmp(order, "row") != 0 && strcmp(order, "col") != 0) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    matrix = xaligned_alloc(64, rows * cols * sizeof(*matrix));
    for (r = 0; r < rows; ++r) {
        for (c = 0; c < cols; ++c) {
            matrix[r * cols + c] = (double)((r + c) % 97);
        }
    }

    if (strcmp(order, "row") == 0) {
        for (r = 0; r < rows; ++r) {
            for (c = 0; c < cols; ++c) {
                total += matrix[r * cols + c];
            }
        }
        start = bench_now_seconds();
        for (pass = 0; pass < passes; ++pass) {
            for (r = 0; r < rows; ++r) {
                for (c = 0; c < cols; ++c) {
                    total += matrix[r * cols + c];
                }
            }
        }
        end = bench_now_seconds();
    } else {
        for (c = 0; c < cols; ++c) {
            for (r = 0; r < rows; ++r) {
                total += matrix[r * cols + c];
            }
        }
        start = bench_now_seconds();
        for (pass = 0; pass < passes; ++pass) {
            for (c = 0; c < cols; ++c) {
                for (r = 0; r < rows; ++r) {
                    total += matrix[r * cols + c];
                }
            }
        }
        end = bench_now_seconds();
    }

    sink_total = total;
    elapsed = end - start;
    ns_per_element = elapsed * 1e9 / (double)(rows * cols * passes);

    printf("row_col_traversal,%s,%zu,%zu,%zu,%zu,1,0,%.9f,%.3f\n",
           order,
           rows * cols * sizeof(*matrix),
           rows * cols,
           rows,
           cols,
           elapsed,
           ns_per_element);

    free(matrix);
    return EXIT_SUCCESS;
}
