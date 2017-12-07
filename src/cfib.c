#if _HAVE_C11_THREAD_LOCAL
#include "cfib_C11.h"
#else
#include "cfib.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _CFIB_SYSAPI_POSIX

#ifdef _HAVE_C11_ATOMICS
#include <stdatomic.h>
#else
#warning "Compiler does not support C11 _Atomic, profiling is disabled!"
#endif

#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

#elif defined (_CFIB_SYSAPI_WINDOWS)
    #error "TODO: WINAPI support."
#endif


// @internal Implemented in assembler module
void _cfib_init_stack(unsigned char** sp, void* start_addr, void* args);

static inline uint_fast32_t _get_sys_page_size() {
#ifdef _CFIB_SYSAPI_POSIX
    static uint_fast32_t cached_size = 0;
    if(cached_size == 0)
        cached_size = (uint_fast32_t)sysconf(_SC_PAGE_SIZE);
    return cached_size;
#elif _CFIB_SYSAPI_WINDOWS
    #error "TODO: WINAPI support."
#endif
}

static inline uint_fast32_t _align_size_to_page(uint_fast32_t size) {
    uint_fast32_t size_mod = size % _get_sys_page_size();
    if(size_mod != 0)
        size = size - size_mod + _get_sys_page_size();
    return size;
}

#ifdef _HAVE_C11_ATOMICS
static void*  _prof_map = NULL;

static atomic_flag _prof_map_lock = ATOMIC_FLAG_INIT;
#endif

#ifdef _CFIB_SYSAPI_POSIX
static pthread_once_t _prof_init_once = PTHREAD_ONCE_INIT;

static void _prof_init() {
    uint_fast32_t pgsz = _get_sys_page_size();
    _prof_map = mmap(0, 1<<16, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
    assert(_prof_map != MAP_FAILED);
}
#elif defined(_CFIB_SYSAPI_WINDOWS)
    #error "TODO: WINAPI support."
#else
    #error "Unsupported platform."
#endif

#ifdef _HAVE_C11_THREAD_LOCAL
_Thread_local cfib_t* _cfib_current = NULL;
#elif defined (_CFIB_SYSAPI_POSIX)
pthread_key_t _cfib_tls_key;

static pthread_once_t _cfib_init_tls_once = PTHREAD_ONCE_INIT;

static void _cfib_init_tls() {
    int ret = pthread_key_create(&_cfib_tls_key, NULL);
    assert("libcfib: pthread_key_create() failed to init TLS key !!!" && ret == 0);
}
#elif defined (_CFIB_SYSAPI_WINDOWS)
    #error "TODO: WINAPI support."
#endif

cfib_t* cfib_init_thread()
{
#if _HAVE_C11_THREAD_LOCAL
    if(_cfib_current != NULL) {
        fprintf(stderr, "libcfib_C11: WARNING: cfib_init_thread() called more than once per thread!\n");
        return _cfib_current;
    }
    _cfib_current = calloc(1, sizeof(cfib_t));
    return _cfib_current;
#elif defined(_CFIB_SYSAPI_POSIX)
    pthread_once(&_cfib_init_tls_once, _cfib_init_tls);
    cfib_t** tls = (cfib_t**)pthread_getspecific(_cfib_tls_key);
    if(tls != NULL) {
        fprintf(stderr, "libcfib: WARNING: cfib_init_thread() called more than once per thread!\n");
        return *tls;
    }
    tls = malloc(sizeof(cfib_t**));
    assert("libcfib: cfib_init_thread() ran out of memory for TLS object !!!" && tls != NULL);
    *tls = calloc(1, sizeof(cfib_t));
    assert("libcfib: cfib_init_thread() ran out of memory for TLS object !!!" && *tls != NULL);
    int ret = pthread_setspecific(_cfib_tls_key, tls);
    assert("libcfib: cfib_init_thread() ran out of memory for TLS object !!!" && ret == 0);
    return *tls;
#elif defined(_CFIB_SYSAPI_WINDOWS)
    #error "TODO: WINAPI support."
#endif
}

cfib_t* cfib_init_thread__prof__()
{
    cfib_t* ret = cfib_init_thread();
#ifdef _HAVE_C11_ATOMICS
    pthread_once(&_prof_init_once, _prof_init);
#else
    fprintf(stderr, "libcfib: WARNING: profiling is disabled!\n");
#endif
    return ret;
}

cfib_t* cfib_new(void* start_routine, void* args, uint_fast32_t ssize)
{
    cfib_t* context = (cfib_t*)calloc(1, sizeof(cfib_t));
    if(context == NULL)
        return NULL;
#ifdef _CFIB_SYSAPI_POSIX
    if(ssize != 0)
        ssize = _align_size_to_page(ssize);
    else
        ssize = CFIB_DEF_STACK_SIZE;
#ifndef __FreeBSD__
    uint_fast32_t page_size = _get_sys_page_size();
#endif
#if defined(__FreeBSD__)
    int mmap_flags = MAP_STACK|MAP_PRIVATE;
#else
    int mmap_flags = MAP_ANONYMOUS|MAP_PRIVATE;
#endif
#if defined(__FreeBSD__)
    void *m = mmap(0, ssize, PROT_READ|PROT_WRITE, mmap_flags, -1, 0);
#else
    void *m = mmap(0, ssize + page_size, PROT_READ|PROT_WRITE, mmap_flags, -1, 0);
#endif
    //assert("libcfib: cfib_init() failed to mmap() stack !!!" && m != MAP_FAILED);
    if(m == MAP_FAILED) {
        fprintf(stderr, "libcfib: WARNING: cfib_new() failed to allocate stack!\n");
        goto _errexit;
    }
#ifndef __FreeBSD__
#if defined(__linux__)
    void *gp = mmap(m, page_size, PROT_NONE, mmap_flags|MAP_FIXED|MAP_NORESERVE, -1, 0);
#else
    void *gp = mmap(m, page_size, PROT_NONE, mmap_flags|MAP_FIXED, -1, 0);
#endif
    if(gp == MAP_FAILED)
        fprintf(stderr, "libcfib: WARNING: cfib_new() failed to mmap() guard page for stack!\n");
    m += page_size;
#endif /* #ifndef __FreeBSD__  */
    context->stack_ceiling = (unsigned char*)m;
    context->sp = context->stack_floor = context->stack_ceiling + ssize;
    _cfib_init_stack(&context->sp, start_routine, args);
    return context;
#elif defined (_CFIB_SYSAPI_WINDOWS)
    #error "TODO: WINAPI support."
#endif
_errexit:
    free(context);
    return NULL;
}

cfib_t* cfib_new__prof__(void* start_routine, void* args, uint_fast32_t ssize)
{
    cfib_t* context = (cfib_t*)calloc(1, sizeof(cfib_t));
    if(context == NULL)
        return NULL;
#ifdef _HAVE_C11_ATOMICS
    void *stack_ceiling = NULL;
    void* vres = NULL;
    int res = posix_memalign(&stack_ceiling, 1<<20, 1<<20);
    if(res != 0) {
        fprintf(stderr, "libcfib: cfib_init__prof__() failed to allocate profiled stack !!!");
        goto _errexit;
    }
    res = mprotect(stack_ceiling, (1<<20) - _get_sys_page_size(), PROT_NONE);
    assert("libcfib: cfib_init__prof__() failed to apply guard on profiled stack !!!" && res == 0);
    context->stack_ceiling = (unsigned char*)stack_ceiling;
    context->sp = context->stack_floor = context->stack_ceiling + (1<<20);
    _cfib_init_stack(&context->sp, start_routine, args);
    while(atomic_flag_test_and_set_explicit(&_prof_map_lock, memory_order_relaxed))
        ; /* Spin until lock is acquired */
    uintptr_t i0 = ((uintptr_t)stack_ceiling>>12) & 0xffff;
    uintptr_t i1 = ((uintptr_t)stack_ceiling>>(12 + 16)) & 0xffff;
    void** map0 = (void**)_prof_map;
    if( map0[i0] == NULL ) {
        vres = map0[i0] = mmap(0, 1<<16, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
        assert("libcfib: cfib_init__prof__() failed to assert profiler stack map !!!" && vres != MAP_FAILED);
    }
    void** map1 = (void**)(map0[i0]);
    map1[i1] = stack_ceiling;
    atomic_flag_clear_explicit(&_prof_map_lock, memory_order_release);
    return context;
#else
    fprintf(stderr, "libcfib: WARNING: profiling is disabled!\n");
    return cfib_new(start_routine, args, ssize);
#endif
_errexit:
    free(context);
    return NULL;
}

void cfib_unmap(cfib_t* context) {
#ifdef _CFIB_SYSAPI_POSIX
#ifdef __FreeBSD__
    munmap(context->stack_ceiling, (size_t)(context->stack_floor - context->stack_ceiling));
#else
    context->stack_ceiling -= _get_sys_page_size();
    munmap(context->stack_ceiling, (size_t)(context->stack_floor - context->stack_ceiling));
#endif
#elif _CFIB_SYSAPI_WINDOWS
    #error "TODO: WINAPI support."
#endif
    memset(context, 0, sizeof(cfib_t));
}

// @internal This function is called from assembler if a fiber returns.
void _cfib_exit_thread() {
#ifdef _CFIB_SYSAPI_POSIX
    pthread_exit(NULL);
#elif defined (_CFIB_SYSAPI_WINDOWS)
    #error "TODO: WINAPI support."
#endif
}

#ifndef _HAVE_C11_THREAD_LOCAL

cfib_t* cfib_get_current() {
#ifdef _CFIB_SYSAPI_POSIX
    cfib_t** tls = (cfib_t**)pthread_getspecific(_cfib_tls_key);
    assert("CALL cfib_init_thread() BEFORE CALLING cfib_get_current() !!!" && tls != NULL);
    return *tls;
#elif defined (_CFIB_SYSAPI_WINDOWS)
    #error "TODO: WINAPI support."
#endif
}

cfib_t* cfib_get_current__noassert__() {
#ifdef _CFIB_SYSAPI_POSIX
    //fprintf(stderr, "cfib_get_current__noassert__()\n");
    return *((cfib_t**)pthread_getspecific(_cfib_tls_key));
#elif defined (_CFIB_SYSAPI_WINDOWS)
    #error "TODO: WINAPI support."
#endif
}

void _cfib_swap(unsigned char** sp1, unsigned char* sp2);

void cfib_swap(cfib_t *to) {
#ifdef _CFIB_SYSAPI_POSIX
    cfib_t** tls = (cfib_t**)pthread_getspecific(_cfib_tls_key);
    assert("CALL cfib_init_thread() BEFORE CALLING cfib_swap() !!!" && tls != NULL);
    cfib_t* from = *tls;
    *tls = to;
    _cfib_swap(&from->sp, to->sp);
#elif defined (_CFIB_SYSAPI_WINDOWS)
    #error "TODO: WINAPI support."
#endif
}

void cfib_swap__noassert__(cfib_t *to) {
#ifdef _CFIB_SYSAPI_POSIX
    //fprintf(stderr, "cfib_swap__noassert__()\n");
    cfib_t** tls = (cfib_t**)pthread_getspecific(_cfib_tls_key);
    cfib_t* from = *tls;
    *tls = to;
    _cfib_swap(&from->sp, to->sp);
#elif defined (_CFIB_SYSAPI_WINDOWS)
    #error "TODO: WINAPI support."
#endif
}

#endif /* #ifndef _HAVE_C11_THREAD_LOCAL  */
