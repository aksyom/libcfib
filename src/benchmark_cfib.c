#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#if _HAVE_C11_THREAD_LOCAL
#include "cfib_C11.h"
#else
#include "cfib.h"
#endif

#define NUM_SAMPLES 1000000

cfib_t *main_context;

void test_fiber(void *arg) {
    while(1)
        cfib_swap__noassert__(main_context);
}

void test_ptrcall(void *arg) {
    return;
}

int _cmp(const void* a, const void *b) {
    return (int)( *((long*)a) - *((long*)b) );
}

long _get_median(long* b, size_t s) {
    long *m = b + s / 2;
    if(s % 2 == 0)
        return (*m + *(m - 1)) / 2;
    else
        return *m;
}
void bench_ptrcall(long *intervals, int n) {
    struct timespec tp0, tp1;
    void (*func)(void*);
    func = test_ptrcall;
    for(int i = 0; i < n; i++) {
        clock_gettime(CLOCK_MONOTONIC, &tp0);
        func(NULL);
        clock_gettime(CLOCK_MONOTONIC, &tp1);
        intervals[i] = tp1.tv_nsec - tp0.tv_nsec;
    }
    qsort(intervals, n, sizeof(long), _cmp);
    printf("median\t%ld ns\n", _get_median(intervals, n));
    printf("   min\t%ld ns\n", intervals[0]);
    printf("   max\t%ld ns\n", intervals[n - 1]);
}

void bench_swap(long *intervals, int n) {
    struct timespec tp0, tp1;
    main_context = cfib_init_thread();
    cfib_t* test_context = cfib_new((void*)test_fiber, NULL, 0);
    for(int i = 0; i < n; i++) {
        clock_gettime(CLOCK_MONOTONIC, &tp0);
        cfib_swap__noassert__(test_context);
        clock_gettime(CLOCK_MONOTONIC, &tp1);
        intervals[i] = tp1.tv_nsec - tp0.tv_nsec;
    }
    qsort(intervals, n, sizeof(long), _cmp);
    printf("median\t%ld ns\n", _get_median(intervals, n));
    printf("   min\t%ld ns\n", intervals[0]);
    printf("   max\t%ld ns\n", intervals[n - 1]);
}

int main(int argc, char** argv) {
    long *intervals = mmap(0, sizeof(long) * NUM_SAMPLES, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
    printf("ptrcall\n");
    bench_ptrcall(intervals, NUM_SAMPLES);
    printf("cfib_swap\n");
    bench_swap(intervals, NUM_SAMPLES);
    return 0;
}
