#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <sys/mman.h>

#if _TEST_C11_BUILD
#include "cfib_C11.h"
#else
#include "cfib.h"
#endif

cfib_t *main_context;

#define NUM_SAMPLES 10000

cfib_t *fib_main;

void test_pingpong(void *arg) {
    while(1)
        cfib_swap((cfib_t*)arg);
}

void test_pingpong__noassert__(void *arg) {
    while(1)
        cfib_swap__noassert__((cfib_t*)arg);
}

void test_return(void *arg) {
    return;
}

int _long_cmp(const void* a, const void *b) {
    return (int)( *((long*)a) - *((long*)b) );
}

long _get_median(long* b, size_t s) {
    long *m = b + s / 2;
    if(s % 2 == 0)
        return (*m + *(m - 1)) / 2;
    else
        return *m;
}

/*void bench_ptrcall(long *intervals, int n) {
    struct timespec tp0, tp1;
    void (*func)(void*);
    func = test_return;
    for(int i = 0; i < n; i++) {
        clock_gettime(CLOCK_MONOTONIC, &tp0);
        func(NULL);
        clock_gettime(CLOCK_MONOTONIC, &tp1);
        intervals[i] = tp1.tv_nsec - tp0.tv_nsec;
    }
    qsort(intervals, n, sizeof(long), _long_cmp);
    printf("median\t%ld ns\n", _get_median(intervals, n));
    printf("   min\t%ld ns\n", intervals[0]);
    printf("   max\t%ld ns\n", intervals[n - 1]);
}*/

void bench_swap(int n) {
    struct timespec tp0, tp1;
    cfib_t* test_context = cfib_new((void*)test_pingpong, (void*)fib_main, 0);
    long *intervals = mmap(0, sizeof(long) * n, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp0);
    long tt = tp0.tv_nsec;
    for(int i = 0; i < n; i++) {
        clock_gettime(CLOCK_MONOTONIC, &tp0);
        cfib_swap(test_context);
        clock_gettime(CLOCK_MONOTONIC, &tp1);
        intervals[i] = tp1.tv_nsec - tp0.tv_nsec;
    }
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp0);
    tt = tp0.tv_nsec - tt;
    qsort(intervals, n, sizeof(long), _long_cmp);
    printf("median\t%ld ns\n", _get_median(intervals, n));
    printf("   min\t%ld ns\n", intervals[0]);
    printf("   max\t%ld ns\n", intervals[n - 1]);
    printf(" total\t%ld ns\n", tt);
    munmap(intervals, sizeof(long) * n);
}

void bench_swap__noassert__(int n) {
    struct timespec tp0, tp1;
    cfib_t* test_context = cfib_new((void*)test_pingpong__noassert__, (void*)fib_main, 0);
    long *intervals = mmap(0, sizeof(long) * n, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp0);
    long tt = tp0.tv_nsec;
    for(int i = 0; i < n; i++) {
        clock_gettime(CLOCK_MONOTONIC, &tp0);
        cfib_swap__noassert__(test_context);
        clock_gettime(CLOCK_MONOTONIC, &tp1);
        intervals[i] = tp1.tv_nsec - tp0.tv_nsec;
    }
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp0);
    tt = tp0.tv_nsec - tt;
    qsort(intervals, n, sizeof(long), _long_cmp);
    printf("median\t%ld ns\n", _get_median(intervals, n));
    printf("   min\t%ld ns\n", intervals[0]);
    printf("   max\t%ld ns\n", intervals[n - 1]);
    printf(" total\t%ld ns\n", tt);
    munmap(intervals, sizeof(long) * n);
}

void print_usage(char *name) {
    fprintf(stderr, "Usage: %s <#>\n\n", name);
    fprintf(stderr, "#\tTest\n");
    fprintf(stderr, "1\tbenchmark\n");
    fprintf(stderr, "2\tbenchmark (noassert)\n");
    fprintf(stderr, "3\tstack hog (NOT IMPLEMENTED)\n");
}

int main(int argc, char** argv) {
    if(argc < 2)
        goto errexit;
    unsigned long test_num = strtoul(argv[1], NULL, 10);
    fib_main = cfib_init_thread();
    cfib_t* fib = NULL;
    long *intervals = NULL;
    switch(test_num) {
        case 1:
            bench_swap(NUM_SAMPLES);
            break;
        case 2:
            bench_swap__noassert__(NUM_SAMPLES);
            break;
        case 3:
            fprintf(stderr, "TODO\n");
            break;
        default:
            goto errexit;
    }
exit:
    return 0;
errexit:
    print_usage(argv[0]);
    return 1;
}
