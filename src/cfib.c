#include "cfib.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WITH_SYSAPI_POSIX

#ifdef _WITH_C11_ATOMICS
#include <stdatomic.h>
#else
#warning "Compiler does not support C11 _Atomic, profiling is disabled!"
#endif

#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include <signal.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

#elif defined (_WITH_SYSAPI_WINDOWS)
    #error "TODO: WINAPI support."
#endif


// @internal Implemented in assembler module
void _cfib_init_stack(unsigned char** sp, void* start_addr, void* args);

static inline uint_fast32_t _get_sys_page_size()
{
    static uint_fast32_t cached_size = 0;
#ifdef _WITH_SYSAPI_POSIX
    if(cached_size == 0)
        cached_size = (uint_fast32_t)sysconf(_SC_PAGE_SIZE);
#elif defined(_WITH_SYSAPI_WINDOWS)
    #error "TODO: WINAPI support."
#endif
    return cached_size;
}

static inline uint_fast32_t _align_size_to_page(uint_fast32_t size)
{
    uint_fast32_t size_mod = size % _get_sys_page_size();
    if(size_mod != 0)
        size = size - size_mod + _get_sys_page_size();
    return size;
}

#ifdef _PROFILED_BUILD
static void*  _prof_map = NULL;
static size_t _prof_nfaults = 0;

#ifdef _WITH_C11_ATOMICS
static atomic_flag _prof_map_lock = ATOMIC_FLAG_INIT;

#define _atomic_test_and_set(flag) atomic_flag_test_and_set_explicit(flag, memory_order_relaxed)
#define _atomic_clear(flag) atomic_flag_clear_explicit(flag, memory_order_release)
#elif defined (_WITH_SYSAPI_WINDOWS) /* #ifdef _WITH_C11_ATOMICS */
typedef atomic_flag LONG;

static LONG _prof_map_lock = 0;

#define _atomic_test_and_set(flag) InterlockedExchange(flag, 1)
#define _atomic_clear(flag) InterlockedExchange(flag, 0)
#else /* #ifdef _WITH_C11_ATOMICS */
void* _prof_map_lock = NULL;

inline static int _atomic_test_and_set(void *flag) {
    assert("_atomic_test_and_set() unimplemented!" && 0);
}

inline static int _atomic_clear(void *flag) {
    assert("_atomic_clear() unimplemented!" && 0);
}
#endif /* #ifdef _WITH_C11_ATOMICS */

#ifdef _WITH_SYSAPI_POSIX
static pthread_once_t _prof_init_once = PTHREAD_ONCE_INIT;

static struct sigaction _prof_oact;

void _prof_segv_handler(int sig, siginfo_t *nfo, void *uap) {
    if(sig != SIGSEGV)
        goto next;
    if(nfo->si_addr == NULL)
        goto next;
    int page_size = _get_sys_page_size();
    void *stk_addr = nfo->si_addr - ((uintptr_t)nfo->si_addr % (1<<20));
    void *page_addr = nfo->si_addr - ((uintptr_t)nfo->si_addr % page_size);
    uintptr_t i0 = ((uintptr_t)stk_addr>>12) & 0xffff;
    uintptr_t i1 = ((uintptr_t)stk_addr>>(12 + 16)) & 0xffff;
    fprintf(stderr, "_prof_segv_handler(): stk_addr = %p, page_addr= %p\n", stk_addr, page_addr);
    fprintf(stderr, "_prof_segv_handler(): i0 = %lu, i1 = %lu\n", i0, i1);
    //while(atomic_flag_test_and_set_explicit(&_prof_map_lock, memory_order_relaxed));
    void** map0 = (void**)_prof_map;
    fprintf(stderr, "_prof_segv_handler(): map0[%lu] = %p\n", i0, map0[i0]);
    if(map0[i0] == NULL)
        goto next;
    void* prof_data = ((void**)(map0[i0]))[i1];
    fprintf(stderr, "_prof_segv_handler(): prof_data = %p\n", prof_data);
    if(prof_data == NULL)
        goto next;
    if(page_addr == stk_addr) {
        fprintf(stderr, "libcfib: Stack break @ %p\n", page_addr);
        goto next;
    }
    //atomic_flag_clear_explicit(&_prof_map_lock, memory_order_release);
    int res = mprotect(page_addr, page_size, PROT_READ|PROT_WRITE);
    assert("Profiling SIGSEGV handler failed to remove guard page from stack!" && res == 0);
    _prof_nfaults++;
    return;
//next_clear:
//    atomic_flag_clear_explicit(&_prof_map_lock, memory_order_release);
next:
    if(_prof_oact.sa_flags & SA_SIGINFO)
        _prof_oact.sa_sigaction(sig, nfo, uap);
    else
        _prof_oact.sa_handler(sig);
}

static void _prof_init() {
    _prof_map = mmap(0, 1<<16, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
    assert("Profiling was enabled, but profiler map allocation failed!" &&  _prof_map != MAP_FAILED);
    size_t sigstk_size = MINSIGSTKSZ + 16 * _get_sys_page_size();
    void *sigstk_mem = mmap(0, sigstk_size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
    stack_t sigstk = {
        .ss_sp = sigstk_mem,
        .ss_size = sigstk_size,
        .ss_flags = 0
    };
    assert( sigaltstack(&sigstk, NULL) == 0 );
    sigset_t sigmask;
    sigaddset(&sigmask, SIGTSTP);
    sigaddset(&sigmask, SIGCONT);
    sigaddset(&sigmask, SIGCHLD);
    sigaddset(&sigmask, SIGTTIN);
    sigaddset(&sigmask, SIGTTOU);
    sigaddset(&sigmask, SIGIO);
    sigaddset(&sigmask, SIGWINCH);
    sigaddset(&sigmask, SIGINFO);
    struct sigaction act = {
        .sa_sigaction = _prof_segv_handler,
        .sa_flags = SA_SIGINFO|SA_ONSTACK|SA_RESTART,
        .sa_mask = sigmask
    };
    assert( sigaction(SIGSEGV, &act, &_prof_oact) == 0 );
    fprintf(stderr, "libcfib: Fiber stack profiler enabled.\n");
}
#elif defined(_WITH_SYSAPI_WINDOWS) /* #ifdef _WITH_SYSAPI_POSIX */
    #error "TODO: WINAPI support."
#endif /* #ifdef _WITH_SYSAPI_POSIX  */

#endif /* #ifdef _PROFILED_BUILD */

#ifdef _WITH_C11_THREAD_LOCAL
_Thread_local cfib_t* _cfib_current = NULL;
#elif defined (_WITH_SYSAPI_POSIX)
pthread_key_t _cfib_tls_key;

static pthread_once_t _cfib_init_tls_once = PTHREAD_ONCE_INIT;

static void _cfib_init_tls() {
    int ret = pthread_key_create(&_cfib_tls_key, NULL);
    assert("libcfib: pthread_key_create() failed to init TLS key !!!" && ret == 0);
}
#elif defined (_WITH_SYSAPI_WINDOWS)
    #error "TODO: WINAPI TSL support."
#endif

cfib_t* cfib_init_thread()
{
    cfib_t* ret = NULL;
#if _WITH_C11_THREAD_LOCAL
    if(_cfib_current != NULL) {
        fprintf(stderr, "libcfib_C11: WARNING: cfib_init_thread() called more than once per thread!\n");
        return _cfib_current;
    }
    _cfib_current = calloc(1, sizeof(cfib_t));
    ret = _cfib_current;
#elif defined(_WITH_SYSAPI_POSIX)
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
    int res = pthread_setspecific(_cfib_tls_key, tls);
    assert("libcfib: cfib_init_thread() ran out of memory for TLS object !!!" && res == 0);
    ret = *tls;
#elif defined (_WITH_SYSAPI_WINDOWS)
    #error "TODO: WINAPI TLS support."
#endif
#if defined(_PROFILED_BUILD) && defined (_WITH_SYSAPI_POSIX)
    pthread_once(&_prof_init_once, _prof_init);
#elif defined(_PROFILED_BUILD) && defined (_WITH_SYSAPI_WINDOWS)
    #error "TODO: WINAPI profiling support."
#endif
    return ret;
}

cfib_t* cfib_new(void* start_routine, void* args, uint_fast32_t ssize)
{
    cfib_t* ret = (cfib_t*)calloc(1, sizeof(cfib_t));
    if(ret == NULL)
        return NULL;
#ifdef _PROFILED_BUILD

    void *stack_ceiling = NULL;
    void* vres = NULL;
    int res = posix_memalign(&stack_ceiling, 1<<20, 1<<20);
    if(res != 0) {
        fprintf(stderr, "libcfib: cfib_init__prof__() failed to allocate profiled stack !!!");
        goto _errexit;
    }
    res = mprotect(stack_ceiling, (1<<20) - _get_sys_page_size(), PROT_NONE);
    assert("libcfib: cfib_init__prof__() failed to apply guard on profiled stack !!!" && res == 0);
    ret->stack_ceiling = (unsigned char*)stack_ceiling;
    ret->sp = ret->stack_floor = ret->stack_ceiling + (1<<20);
    _cfib_init_stack(&ret->sp, start_routine, args);
    while( _atomic_test_and_set(&_prof_map_lock) )
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
    _atomic_clear(&_prof_map_lock);
    fprintf(stderr, "__prof__: i0 = %lu, i1 = %lu, map0[%lu] = %p, map1[%lu] = %p\n", i0, i1, i0, map0[i0], i1, map1[i1]);
    fprintf(stderr, "libcfib: New profiled stack @ [%p - %p]\n", (void*)ret->stack_ceiling, (void*)ret->stack_floor);

#else /* #ifdef _PROFILED_BUILD  */

#ifdef _WITH_SYSAPI_POSIX
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
    assert("Failed to set guard page!" && mprotect(m, page_size, PROT_NONE) == 0);
    m += page_size;
#endif
    ret->stack_ceiling = (unsigned char*)m;
    ret->sp = ret->stack_floor = ret->stack_ceiling + ssize;
    _cfib_init_stack(&ret->sp, start_routine, args);
#elif defined (_WITH_SYSAPI_WINDOWS)
    #error "TODO: WINAPI support."
#endif

#endif /* #ifdef _PROFILED_BUILD */
_exit:
    return ret;
_errexit:
    free(ret);
    return NULL;
}

void cfib_unmap(cfib_t* context) {
#ifdef _WITH_SYSAPI_POSIX
#ifdef __FreeBSD__
    munmap(context->stack_ceiling, (size_t)(context->stack_floor - context->stack_ceiling));
#else
    context->stack_ceiling -= _get_sys_page_size();
    munmap(context->stack_ceiling, (size_t)(context->stack_floor - context->stack_ceiling));
#endif
#elif _WITH_SYSAPI_WINDOWS
    #error "TODO: WINAPI support."
#endif
    memset(context, 0, sizeof(cfib_t));
}

// @internal This function is called from assembler if a fiber returns.
void _cfib_exit_thread() {
#ifdef _WITH_SYSAPI_POSIX
    pthread_exit(NULL);
#elif defined (_WITH_SYSAPI_WINDOWS)
    #error "TODO: WINAPI support."
#endif
}


cfib_t* cfib_get_current() {
#ifdef _WITH_C11_THREAD_LOCAL
    assert("CALL cfib_init_thread() BEFORE CALLING cfib_get_current() !!!" && _cfib_current != NULL);
    return _cfib_current;
#elif defined (_WITH_SYSAPI_POSIX)
    cfib_t** tls = (cfib_t**)pthread_getspecific(_cfib_tls_key);
    assert("CALL cfib_init_thread() BEFORE CALLING cfib_get_current() !!!" && tls != NULL);
    return *tls;
#elif defined (_WITH_SYSAPI_WINDOWS)
    #error "TODO: WINAPI support."
#endif
}

cfib_t* cfib_get_current__noassert__() {
#ifdef _WITH_C11_THREAD_LOCAL
    return _cfib_current;
#elif defined (_WITH_SYSAPI_POSIX)
    return *((cfib_t**)pthread_getspecific(_cfib_tls_key));
#elif defined (_WITH_SYSAPI_WINDOWS)
    #error "TODO: WINAPI support."
#endif
}

void _cfib_swap(unsigned char** sp1, unsigned char* sp2);

void cfib_swap(cfib_t *to) {
#ifdef _WITH_C11_THREAD_LOCAL
    assert("CALL cfib_init_thread() BEFORE CALLING cfib_swap() !!!" && _cfib_current != NULL);
    cfib_t* from = _cfib_current;
    _cfib_current = to;
    _cfib_swap(&from->sp, to->sp);
#elif defined (_WITH_SYSAPI_POSIX)
    cfib_t** tls = (cfib_t**)pthread_getspecific(_cfib_tls_key);
    assert("CALL cfib_init_thread() BEFORE CALLING cfib_swap() !!!" && tls != NULL);
    cfib_t* from = *tls;
    *tls = to;
    _cfib_swap(&from->sp, to->sp);
#elif defined (_WITH_SYSAPI_WINDOWS)
    #error "TODO: WINAPI support."
#endif
}

void cfib_swap__noassert__(cfib_t *to) {
#ifdef _WITH_C11_THREAD_LOCAL
    cfib_t* from = _cfib_current;
    _cfib_current = to;
    _cfib_swap(&from->sp, to->sp);
#elif defined (_WITH_SYSAPI_POSIX)
    cfib_t** tls = (cfib_t**)pthread_getspecific(_cfib_tls_key);
    cfib_t* from = *tls;
    *tls = to;
    _cfib_swap(&from->sp, to->sp);
#elif defined (_WITH_SYSAPI_WINDOWS)
    #error "TODO: WINAPI support."
#endif
}

