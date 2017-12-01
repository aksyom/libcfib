#include "cfib.h"

// C11 stuff
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

// Unix stuff
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>

// Implemented in assembler module
void _cfib_init_stack(unsigned char** sp, void* start_addr, void* args);

static inline uint_fast32_t _cfib_pgsz() {
    static uint_fast32_t pgsz = 0;
    if(pgsz == 0)
    pgsz = (uint_fast32_t)sysconf(_SC_PAGE_SIZE);
    return pgsz;
}

_Thread_local cfib_t* cfib_current = NULL;

cfib_t* cfib_init_thread()
{
    if(cfib_current != NULL) {
        fprintf(stderr, "libcfib: cfib_init_thread() called from an initialized context!");
        assert(cfib_current == NULL);
    }
    cfib_current = calloc(1, sizeof(cfib_t));
    return cfib_current;
}

cfib_t* cfib_init(cfib_t* context, void* start_routine, void* args, uint_fast32_t ssize)
{
    if(ssize != 0) {
        uint_fast32_t size_mod = ssize % _cfib_pgsz();
        if(size_mod != 0)
            ssize = ssize - size_mod + _cfib_pgsz();
    } else
        ssize = CFIB_DEF_STACK_SIZE;
    void *m = mmap(0, ssize, PROT_READ|PROT_WRITE, MAP_STACK|MAP_PRIVATE, -1, 0);
    if(m == MAP_FAILED) {
        fprintf(stderr, "libcfib: cfib_init() failed to mmap() stack.\n");
        assert(m != MAP_FAILED);
    }
    context->stack_ceiling = (unsigned char*)m;
    context->sp = context->stack_floor = context->stack_ceiling + ssize;
    _cfib_init_stack(&context->sp, start_routine, args);
    return context;
}

void cfib_unmap(cfib_t* context) {
    munmap(context->stack_ceiling, (size_t)(context->stack_floor - context->stack_ceiling));
    memset(context, 0, sizeof(cfib_t));
}

// This function is called from assembler if a fiber returns.
void _cfib_exit_thread() {
    pthread_exit(NULL);
}
