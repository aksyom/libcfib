#if _HAVE_C11_THREAD_LOCAL
#include "cfib_C11.h"
#else
#include "cfib.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _CFIB_SYSAPI_POSIX
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#elif defined (_CFIB_SYSAPI_WINDOWS)
    #error "TODO: WINAPI support."
#endif

// @internal Implemented in assembler module
void _cfib_init_stack(unsigned char** sp, void* start_addr, void* args);

static inline uint_fast32_t _cfib_pgsz() {
#ifdef _CFIB_SYSAPI_POSIX
    static uint_fast32_t pgsz = 0;
    if(pgsz == 0)
        pgsz = (uint_fast32_t)sysconf(_SC_PAGE_SIZE);
    return pgsz;
#elif _CFIB_SYSAPI_WINDOWS
    #error "TODO: WINAPI support."
#endif
}

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
        fprintf(stderr, "libcfib_C11: WARNING: cfib_init_thread() called more than once per thread!");
        return _cfib_current;
    }
    _cfib_current = calloc(1, sizeof(cfib_t));
    return _cfib_current;
#elif defined(_CFIB_SYSAPI_POSIX)
    pthread_once(&_cfib_init_tls_once, _cfib_init_tls);
    cfib_t** tls = (cfib_t**)pthread_getspecific(_cfib_tls_key);
    if(tls != NULL) {
        fprintf(stderr, "libcfib: WARNING: cfib_init_thread() called more than once per thread!");
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

cfib_t* cfib_init(cfib_t* context, void* start_routine, void* args, uint_fast32_t ssize)
{
#ifdef _CFIB_SYSAPI_POSIX
    if(ssize != 0) {
        uint_fast32_t size_mod = ssize % _cfib_pgsz();
        if(size_mod != 0)
            ssize = ssize - size_mod + _cfib_pgsz();
    } else
        ssize = CFIB_DEF_STACK_SIZE;
#ifdef MAP_STACK
    void *m = mmap(0, ssize, PROT_READ|PROT_WRITE, MAP_STACK|MAP_PRIVATE, -1, 0);
#elif defined(MAP_ANONYMOUS)
    void *m = mmap(0, ssize, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
#else
    void *m = mmap(0, ssize, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
#endif
    assert("libcfib: cfib_init() failed to mmap() stack !!!" && m != MAP_FAILED);
    context->stack_ceiling = (unsigned char*)m;
    context->sp = context->stack_floor = context->stack_ceiling + ssize;
    _cfib_init_stack(&context->sp, start_routine, args);
    return context;
#elif defined (_CFIB_SYSAPI_WINDOWS)
    #error "TODO: WINAPI support."
#endif
}

void cfib_unmap(cfib_t* context) {
#ifdef _CFIB_SYSAPI_POSIX
    munmap(context->stack_ceiling, (size_t)(context->stack_floor - context->stack_ceiling));
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
