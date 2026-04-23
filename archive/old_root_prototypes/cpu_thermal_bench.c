#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t stop_flag = 0;
static volatile double sink = 0.0;

static void handle_signal(int signo) {
    (void)signo;
    stop_flag = 1;
}

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s <duration_sec> <intensity_percent> <report_interval_sec>\n"
            "  duration_sec: total runtime in seconds\n"
            "  intensity_percent: 1-100, controls busy duty cycle\n"
            "  report_interval_sec: emit one progress report every N seconds\n",
            prog);
}

static double diff_sec(const struct timespec *a, const struct timespec *b) {
    time_t sec = b->tv_sec - a->tv_sec;
    long nsec = b->tv_nsec - a->tv_nsec;
    if (nsec < 0) {
        sec -= 1;
        nsec += 1000000000L;
    }
    return (double)sec + (double)nsec / 1e9;
}

static void busy_compute(double target_sec) {
    struct timespec start, now;
    double x = sink + 1.0;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &start) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }

    do {
        for (int i = 0; i < 100000; i++) {
            x = x * 1.0000001192092896 + 3.141592653589793;
            x = sqrt(x) + 0.125;
        }
        if (clock_gettime(CLOCK_MONOTONIC_RAW, &now) != 0) {
            perror("clock_gettime");
            exit(EXIT_FAILURE);
        }
    } while (!stop_flag && diff_sec(&start, &now) < target_sec);

    sink = x;
    __asm__ volatile("" : : "m"(sink) : "memory");
}

static void sleep_sec(double sec) {
    if (sec <= 0.0) {
        return;
    }

    struct timespec req;
    req.tv_sec = (time_t)sec;
    req.tv_nsec = (long)((sec - (double)req.tv_sec) * 1e9);

    while (!stop_flag && nanosleep(&req, &req) != 0) {
        if (errno != EINTR) {
            perror("nanosleep");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char **argv) {
    long duration_sec, intensity_pct, report_interval_sec;
    const double epoch_sec = 0.1;
    double busy_sec, idle_sec;
    struct sigaction sa;
    struct timespec t0, now, last_report;
    uint64_t epochs = 0;

    if (argc != 4) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    duration_sec = strtol(argv[1], NULL, 10);
    intensity_pct = strtol(argv[2], NULL, 10);
    report_interval_sec = strtol(argv[3], NULL, 10);

    if (duration_sec <= 0 || intensity_pct < 1 || intensity_pct > 100 || report_interval_sec <= 0) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    busy_sec = epoch_sec * ((double)intensity_pct / 100.0);
    idle_sec = epoch_sec - busy_sec;

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &t0) != 0 ||
        clock_gettime(CLOCK_MONOTONIC_RAW, &last_report) != 0) {
        perror("clock_gettime");
        return EXIT_FAILURE;
    }

    printf("duration_sec=%ld\n", duration_sec);
    printf("intensity_percent=%ld\n", intensity_pct);
    printf("report_interval_sec=%ld\n", report_interval_sec);
    printf("epoch_sec=%.3f\n", epoch_sec);
    printf("busy_sec=%.6f\n", busy_sec);
    printf("idle_sec=%.6f\n", idle_sec);
    fflush(stdout);

    while (!stop_flag) {
        busy_compute(busy_sec);
        sleep_sec(idle_sec);
        epochs++;

        if (clock_gettime(CLOCK_MONOTONIC_RAW, &now) != 0) {
            perror("clock_gettime");
            return EXIT_FAILURE;
        }

        double elapsed = diff_sec(&t0, &now);
        double since_report = diff_sec(&last_report, &now);

        if (since_report >= (double)report_interval_sec) {
            printf("elapsed_sec=%.3f epochs=%" PRIu64 " sink=%f\n", elapsed, epochs, sink);
            fflush(stdout);
            last_report = now;
        }

        if (elapsed >= (double)duration_sec) {
            break;
        }
    }

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &now) != 0) {
        perror("clock_gettime");
        return EXIT_FAILURE;
    }

    printf("total_elapsed_sec=%.3f\n", diff_sec(&t0, &now));
    printf("epochs=%" PRIu64 "\n", epochs);
    printf("final_sink=%f\n", sink);
    return EXIT_SUCCESS;
}
