#ifndef _CFIB_H_
#define _CFIB_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/*! Default stack size.
 *
 * This is the constant which defines compile time default size for all created
 * fibers. This default can be overriden by an optional argument when creating
 * or initializing a fiber.
 */
#define CFIB_DEF_STACK_SIZE (1<<16)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _cfib_context_type {
    /*! Stack pointer of a saved context.
     *
     * Just before pivoting stack to new context's stack pointer, the old
     * context's stack pointer will be saved to this stack pointer variable.
     */
    unsigned char* sp;
    /*! The lower boundary of a fiber's stack window.
     *
     * The stack (pointer) can grow (down on most CPUs) up to this boundary,
     * after which system will generate SIGSEGV or similar fault.
     */
    unsigned char* stack_ceiling;
    /*! The upper boundary of a fiber's stack window.
     *
     * Initially each context's stack pointer points to the stack floor.
     * But before a call to the fiber's call vector (function pointer)
     * can be made, specific data will be pushed to the stack, and thus
     * the stack pointer, in practice, never points to stack floor.
     */
    unsigned char* stack_floor;
} cfib_t;

/*! Pointer to current execution context.
 *
 * The data of this pointer will be modified as needed when a swap occurs.
 */
extern _Thread_local cfib_t* cfib_current;

/*! Initialize a fiber context for current thread.
 *
 * This function MUST be called in a thread before any of the other cfib_*
 * functions can be called. Prior to calling cfib_init() in a thread all
 * cfib_*() calls lead program exit with an error message disciplining
 * the programmer who was too proud to read the fucking documentation.
 * 
 * @return a context for this thread's main fiber.
 */
cfib_t* cfib_init_thread();

/*! Initialize the fiber context provided as argument.
 *
 * This function initializes a fiber context structure so that it can be
 * swap():ed into. Specifically, a stack is mapped and initialized so that
 * when context is swap():ed into, the function 'start_routine' get called
 * with 'args' as it's 1st and only argument.
 *
 * The argument 'start_routine' must be a pointer to a function which has
 * the signature of void (*)(TYPE *), where TYPE is any type you desire.
 * In layman terms, the function for 'start_routine'
 * - must take only one pointer (of any type) as an argument
 * - has a return type of void
 *
 * The argument 'start_routine' should always be explicitly cast to void*
 *
 * This function returns the same context pointer that was passed as argument.
 *
 * @param[out] context pointer to the context to be initialized, need not be zeroed.
 * @param[in] start_routine a pointer to a function to be executed when cfib_swap() is called on this context.
 * @param[in] args pointer to argument data passed as 1st argument to start_routine.
 * @param[in] ssize the maximum size of the stack, will be automatically rounded to page boundary.
 * @return the context pointer provided as first argument.
 */
cfib_t* cfib_init(cfib_t* context, void* start_routine, void* args, uint32_t ssize);

/*! Allocate and initialize a new fiber context.
 *
 * This is just a convenience wrapper that allocates a new cfib_t*
 * context pointer and then passes all the arguments to cfib_init(), then
 * returns the allocated and initialized context.
 *
 * Read the documentation for cfib_init() for a more detailed explanation.
 *
 * @param[in] start_routine a pointer to a function to be executed when cfib_swap() is called on this context.
 * @param[in] args pointer to argument data passed as 1st argument to start_routine.
 * @param[in] ssize the maximum size of the stack, will be automatically rounded to page boundary.
 */
inline cfib_t* cfib_new(void* start_routine, void* args, uint32_t ssize) {
    cfib_t* c = malloc(sizeof(cfib_t));
    cfib_init(c, start_routine, args, ssize);
    return c;
}

// Do not use this function directly. Later I'll probably implement cfib_swap()
// entirely on assembler, and there will be no need for this extra step.
void _cfib_swap(unsigned char** sp1, unsigned char* sp2);

/*! Swap to another execution context.
 *
 * Swaps current execution context to the one provided as an argument. That is,
 * save execution state to cfib_current, then swap to provided context and return
 * executing where it left off.
 *
 * @param[in/out] to_context the context from which execution should continue.
 *
 * @remark It is ENTIRELY up to the programmer to keep tabs on different contexts.
 */
inline void cfib_swap(cfib_t* to) {
    cfib_t* from = cfib_current;
    cfib_current = to;
    fprintf(stderr, "%p -> %p\n", from, to);
    fflush(stderr);
    _cfib_swap(&from->sp, cfib_current->sp);
}

/*! Unmap the stack memory of the provided context.
 *
 * Upon calling cfib_free() on a fiber context, it's stack (and ONLY the stack)
 * is unmapped and the fiber can no longer be swap():ed into. The consideration
 * of where the original void *args -argument of the initial call to
 * cfib_init()/cfib_new() is freed is left to the user; likewise, this
 * function will NOT free the context pointer itself!
 *
 * @param[in/out] context the fiber context the stack of which is to be unmapped.
 */
void cfib_unmap(cfib_t* context);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* _CFIB_H_  */
