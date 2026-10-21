#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>

#define INSERTION_CUTOFF 32
#define PAR_CUTOFF 50000

static int max_threads = 1;
static int active = 1;
static int peak = 1;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

static int try_acquire(void)
{
    int ok = 0;
    pthread_mutex_lock(&mtx);
    if (active < max_threads) {
        active++;
        if (active > peak) peak = active;
        ok = 1;
    }
    pthread_mutex_unlock(&mtx);
    return ok;
}

static void release(void)
{
    pthread_mutex_lock(&mtx);
    active--;
    pthread_mutex_unlock(&mtx);
}

static void reset_counters(void)
{
    pthread_mutex_lock(&mtx);
    active = 1;
    peak = 1;
    pthread_mutex_unlock(&mtx);
}

static inline void swap_int(int *x, int *y)
{
    int t = *x; *x = *y; *y = t;
}

static void insertion_sort(int *a, long lo, long hi)
{
    for (long i = lo + 1; i <= hi; i++) {
        int v = a[i];
        long j = i - 1;
        while (j >= lo && a[j] > v) { a[j + 1] = a[j]; j--; }
        a[j + 1] = v;
    }
}

static long partition(int *a, long lo, long hi)
{
    long mid = lo + (hi - lo) / 2;
    if (a[lo] > a[mid]) swap_int(&a[lo], &a[mid]);
    if (a[lo] > a[hi])  swap_int(&a[lo], &a[hi]);
    if (a[mid] > a[hi]) swap_int(&a[mid], &a[hi]);
    int p = a[mid];
    long i = lo - 1, j = hi + 1;
    for (;;) {
        do i++; while (a[i] < p);
        do j--; while (a[j] > p);
        if (i >= j) return j;
        swap_int(&a[i], &a[j]);
    }
}

typedef struct { int *a; long lo, hi; } task_t;

static void qsort_par(int *a, long lo, long hi);

static void *thread_main(void *arg)
{
    task_t *t = (task_t *)arg;
    int *a = t->a;
    long lo = t->lo, hi = t->hi;
    free(t);
    qsort_par(a, lo, hi);
    release();
    return NULL;
}

static void qsort_par(int *a, long lo, long hi)
{
    while (lo < hi) {
        if (hi - lo + 1 <= INSERTION_CUTOFF) {
            insertion_sort(a, lo, hi);
            return;
        }
        long j = partition(a, lo, hi);

        if (hi - lo + 1 >= PAR_CUTOFF && try_acquire()) {
            task_t *t = malloc(sizeof *t);
            if (t) {
                t->a = a; t->lo = lo; t->hi = j;
                pthread_t th;
                int rc = pthread_create(&th, NULL, thread_main, t);
                if (rc == 0) {
                    qsort_par(a, j + 1, hi);
                    pthread_join(th, NULL);
                    return;
                }
                free(t);
            }
            release();
        }
        if (j - lo < hi - j) { qsort_par(a, lo, j); lo = j + 1; }
        else                 { qsort_par(a, j + 1, hi); hi = j; }
    }
}

static double now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static unsigned long long rng_state;
static unsigned long long rng(void)
{
    rng_state ^= rng_state >> 12;
    rng_state ^= rng_state << 25;
    rng_state ^= rng_state >> 27;
    return rng_state * 2685821657736338717ULL;
}

static void fill(int *a, long n, const char *mode)
{
    for (long i = 0; i < n; i++) {
        if (!strcmp(mode, "sorted"))       a[i] = (int)(i % INT_MAX);
        else if (!strcmp(mode, "reverse")) a[i] = (int)((n - i) % INT_MAX);
        else if (!strcmp(mode, "equal"))   a[i] = 42;
        else                               a[i] = (int)(rng() >> 33);
    }
}

static int is_sorted(const int *a, long n)
{
    for (long i = 1; i < n; i++)
        if (a[i - 1] > a[i]) return 0;
    return 1;
}

int main(int argc, char **argv)
{
    long n = 10000000;
    int repeats = 3, wait_sec = 0;
    unsigned long long seed = 12345;
    const char *mode = "random";

    int opt;
    while ((opt = getopt(argc, argv, "t:n:s:m:r:w:")) != -1) {
        switch (opt) {
        case 't': max_threads = atoi(optarg); break;
        case 'n': n = atol(optarg); break;
        case 's': seed = strtoull(optarg, NULL, 10); break;
        case 'm': mode = optarg; break;
        case 'r': repeats = atoi(optarg); break;
        case 'w': wait_sec = atoi(optarg); break;
        default:
            fprintf(stderr,
                "Usage: %s -t threads -n size [-s seed] [-m random|sorted|reverse|equal] [-r repeats] [-w sec]\n",
                argv[0]);
            return 1;
        }
    }
    if (max_threads < 1 || n < 1 || repeats < 1) {
        fprintf(stderr, "Некорректные параметры\n");
        return 1;
    }
    if (n > (long)INT_MAX) {
        fprintf(stderr, "n слишком велико для int-элементов: максимум %d\n", INT_MAX);
        return 1;
    }

    int *a = malloc((size_t)n * sizeof(int));
    if (!a) { perror("malloc"); return 1; }

    printf("PID=%d, max_threads=%d, n=%ld, mode=%s\n", getpid(), max_threads, n, mode);
    fflush(stdout);

    double total = 0;
    for (int r = 0; r < repeats; r++) {
        rng_state = seed + (unsigned long long)r;
        fill(a, n, mode);
        reset_counters();

        double t0 = now();
        qsort_par(a, 0, n - 1);
        double dt = now() - t0;

        if (!is_sorted(a, n)) {
            fprintf(stderr, "ОШИБКА: массив не отсортирован\n");
            free(a);
            return 2;
        }
        printf("run %d: %.4f s, пик потоков = %d\n", r + 1, dt, peak);
        total += dt;
    }
    printf("RESULT threads=%d n=%ld mode=%s avg_time=%.6f\n",
           max_threads, n, mode, total / repeats);

    if (wait_sec > 0) {
        printf("Пауза %d с, PID=%d...\n", wait_sec, getpid());
        fflush(stdout);
        sleep(wait_sec);
    }
    free(a);
    return 0;
}
