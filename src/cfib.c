#include "cfib.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _PROFILED_BUILD

#ifdef _WITH_C11_ATOMICS
#include <stdatomic.h>
#else
#error "Compiler does not support C11 _Atomic, cannot compile profiling variant of library!"
#endif

#endif /* #ifdef _PROFILED_BUILD  */

#ifdef _WITH_SYSAPI_POSIX

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

_Thread_local struct _cfib_tls _cfib_tls = {
    .current = NULL,
    .previous = NULL
};

static cfib_attr_t _default_attr = {
    .stack_size = 1<<16,
    .flags = 0x0,
    .tag = NULL
};

// @internal Implemented in assembler module
void _cfib_init_stack(unsigned char** sp, cfib_func start_addr, void* args);

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
typedef struct {
    const char* _name;
    uintptr_t _magic;
    uint32_t num_faults;
    uint32_t max_stack_size;
    atomic_bool _sync;
} cfib_tag_t;

static cfib_tag_t _default_tag = {._name = "__DEFAULT__"};

#define _atomic_test_and_set(flag) atomic_flag_test_and_set_explicit(flag, memory_order_relaxed)
#define _atomic_clear(flag) atomic_flag_clear_explicit(flag, memory_order_release)

#ifdef _WITH_SYSAPI_POSIX
static pthread_once_t _prof_init_once = PTHREAD_ONCE_INIT;

static struct sigaction _prof_oact;

void _prof_segv_handler(int sig, siginfo_t *nfo, void *uap) {
    if(nfo->si_addr == NULL)
        goto next;
    if(_cfib_tls.current == NULL)
        goto next;
    cfib_t* faulting_fib = NULL;
    cfib_t* fibs[] ={_cfib_tls.current, _cfib_tls.previous};
    for(int i = 0; i < 2; i++) {
        if( nfo->si_addr >= (void*)fibs[i]->stack_ceiling
            && nfo->si_addr < (void*)fibs[i]->stack_floor )
        {
            faulting_fib = fibs[i];
            break;
        }
    }
    if(faulting_fib == NULL)
        goto next;
    void *stack_ceiling = faulting_fib->stack_ceiling;
    void *stack_floor = faulting_fib->stack_floor;
    unsigned page_size = _get_sys_page_size();
    char *page_addr = (char*)nfo->si_addr - ((uintptr_t)nfo->si_addr % page_size);
    if(page_addr == stack_ceiling) {
        fprintf(stderr, "libcfib: Stack break @ %p\n", nfo->si_addr);
        goto next;
    }
    int res = mprotect(page_addr, page_size, PROT_READ|PROT_WRITE);
    assert("Profiling SIGSEGV handler failed to remove guard page from stack!" && res == 0);
    fprintf(stderr, "libcfib: SEGV @ addr = %p, page = [%p:%p), stack = [%p:%p)", nfo->si_addr, page_addr, page_addr + page_size, stack_ceiling, stack_floor);
    assert(faulting_fib->_private != NULL);
    cfib_tag_t* tag = (cfib_tag_t*)faulting_fib->_private;
    fprintf(stderr, ", group = %s", tag->_name);
    unsigned stack_diff = (unsigned)((uintptr_t)stack_floor - (uintptr_t)page_addr);
    while( atomic_exchange_explicit(&tag->_sync, 1, memory_order_acquire) );
    tag->num_faults++;
    if(tag->max_stack_size < stack_diff)
        tag->max_stack_size = stack_diff;
    atomic_store_explicit(&tag->_sync, 0, memory_order_release);
    fputs("\n", stderr);
    return;
//next_clear:
//    atomic_flag_clear_explicit(&_prof_tbl_lock, memory_order_release);
next:
    if(_prof_oact.sa_flags & SA_SIGINFO)
        _prof_oact.sa_sigaction(sig, nfo, uap);
    else
        _prof_oact.sa_handler(sig);
}

static void _prof_init_thread() {
    static _Thread_local int called_before = 0;
    assert(!called_before);
    size_t sigstk_size = MINSIGSTKSZ + 16 * _get_sys_page_size();
    void *sigstk_mem = mmap(0, sigstk_size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
    stack_t sigstk = {
        .ss_sp = sigstk_mem,
        .ss_size = sigstk_size,
        .ss_flags = 0
    };
    assert( sigaltstack(&sigstk, NULL) == 0 );
    called_before = 1;
}

static void _prof_init() {
    sigset_t sigmask;
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGURG);
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
    fprintf(stderr, "libcfib: Fiber profiler enabled.\n");
}


#elif defined(_WITH_SYSAPI_WINDOWS) /* #ifdef _WITH_SYSAPI_POSIX */
    #error "TODO: WINAPI support."
#endif /* #ifdef _WITH_SYSAPI_POSIX  */

#endif /* #ifdef _PROFILED_BUILD */

cfib_t* cfib_init_thread()
{
    static _Thread_local int called_before = 0;
    if(called_before) {
        fprintf(stderr, "libcfib_C11: WARNING: cfib_init_thread() called more than once per thread!\n");
        return _cfib_tls.current;
    }
#if defined(_PROFILED_BUILD) && defined (_WITH_SYSAPI_POSIX)
    _prof_init_thread();
    pthread_once(&_prof_init_once, _prof_init);
#elif defined(_PROFILED_BUILD) && defined (_WITH_SYSAPI_WINDOWS)
    #error "TODO: WINAPI profiling support."
#endif
    _cfib_tls.current = calloc(1, sizeof(cfib_t));
    _cfib_tls.current->_magic = (uintptr_t)_cfib_tls.current ^ _CFIB_MGK1;
    called_before = 1;
    return _cfib_tls.current;
}

cfib_t* cfib_new(cfib_func start_routine, void* args, const cfib_attr_t* attr)
{
    cfib_t* ret = (cfib_t*)calloc(1, sizeof(cfib_t));
    if(ret == NULL)
        return NULL;
    cfib_attr_t _attr;
    unsigned page_size = _get_sys_page_size();
    if(attr != NULL) {
        _attr.stack_size = _align_size_to_page(attr->stack_size);
        if(_attr.stack_size < (2 * page_size))
            _attr.stack_size = 2 * page_size;
        _attr.flags = attr->flags;
        _attr.tag = attr->tag;
        attr = &_attr;
    } else
        attr = &_default_attr;
#ifdef _PROFILED_BUILD
    void *stack_ceiling = NULL;
    int res = posix_memalign(&stack_ceiling, page_size, attr->stack_size);
    if(res != 0) {
        fprintf(stderr, "libcfib: cfib_new() failed to allocate profiled stack !!!");
        goto _errexit;
    }
    res = mprotect(stack_ceiling, attr->stack_size - _get_sys_page_size(), PROT_NONE);
    assert("libcfib: cfib_new() failed to apply guard on profiled stack !!!" && res == 0);
    ret->stack_ceiling = (unsigned char*)stack_ceiling;
    ret->sp = ret->stack_floor = ret->stack_ceiling + attr->stack_size;
    if(attr->tag != NULL)
        ret->_private = (void*)attr->tag();
    else
        ret->_private = (void*)&_default_tag;

#else /* #ifdef _PROFILED_BUILD  */

#ifdef _WITH_SYSAPI_POSIX
#if defined(__FreeBSD__)
    int mmap_flags = MAP_STACK|MAP_PRIVATE;
#else
    int mmap_flags = MAP_ANONYMOUS|MAP_PRIVATE;
#endif
#if defined(__FreeBSD__)
    void *m = mmap(0, attr->stack_size, PROT_READ|PROT_WRITE, mmap_flags, -1, 0);
#else
    void *m = mmap(0, attr->stack_size + page_size, PROT_READ|PROT_WRITE, mmap_flags, -1, 0);
#endif
    if(m == MAP_FAILED) {
        fprintf(stderr, "libcfib: WARNING: cfib_new() failed to mmap() stack!\n");
        goto _errexit;
    }
#ifndef __FreeBSD__
    assert("Failed to set guard page!" && mprotect(m, page_size, PROT_NONE) == 0);
    m += page_size;
#endif
    ret->stack_ceiling = (unsigned char*)m;
    ret->sp = ret->stack_floor = ret->stack_ceiling + attr->stack_size;
#elif defined (_WITH_SYSAPI_WINDOWS)
    #error "TODO: WINAPI support."
#endif

#endif /* #ifdef _PROFILED_BUILD */
    _cfib_init_stack(&ret->sp, start_routine, args);
    ret->_magic = (uintptr_t)ret ^ _CFIB_MGK1;
    return ret;
_errexit:
    if(ret != NULL) free(ret);
    return NULL;
}

void cfib_unmap(cfib_t* context) {
#ifdef _PROFILED_BUILD

    free(context->stack_ceiling);

#else

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
