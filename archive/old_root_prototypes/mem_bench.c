#include <stdio.h>
#include <stdlib.h>

#define N (100 * 1024 * 1024)

static int *arr;

static void sequential(void) {
    volatile long long sum = 0;

    for (int i = 0; i < N; i++) {
        sum += arr[i];
    }
}

static void strided(int stride) {
    volatile long long sum = 0;

    for (int i = 0; i < N; i += stride) {
        sum += arr[i];
    }
}

static void random_access(void) {
    volatile long long sum = 0;

    for (int i = 0; i < N; i++) {
        sum += arr[rand() % N];
    }
}

int main(int argc, char *argv[]) {
    int stride = 64;

    if (argc < 2) {
        fprintf(stderr, "usage: %s [s|t|r] [stride]\n", argv[0]);
        return 1;
    }

    arr = malloc(sizeof(int) * N);
    if (arr == NULL) {
        perror("malloc");
        return 1;
    }

    for (int i = 0; i < N; i++) {
        arr[i] = i;
    }

    srand(42);

    if (argc >= 3) {
        stride = atoi(argv[2]);
        if (stride <= 0) {
            fprintf(stderr, "stride must be > 0\n");
            free(arr);
            return 1;
        }
    }

    if (argv[1][0] == 's') {
        sequential();
    } else if (argv[1][0] == 't') {
        strided(stride);
    } else if (argv[1][0] == 'r') {
        random_access();
    } else {
        fprintf(stderr, "unknown mode: %s\n", argv[1]);
        free(arr);
        return 1;
    }

    free(arr);
    return 0;
}
