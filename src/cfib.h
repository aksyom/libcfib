#ifndef _CFIB_H_
#define _CFIB_H_

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

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

/*! This is the struct that holds execution context for each fiber.
 *
 * In essence, this struct IS the fiber itself, but in documentation we might
 * refer to this struct with words "fiber" or "context" or "execution context".
 *
 * The data of this struct is accessed and modified by all cfib_* functions.
 *
 * To initialize a new fiber, use cfib_init() or cfib_new() functions.
 *
 * To switch into an initialized fiber, use cfib_swap().
 *
 * To destroy a fiber and free it's memory, use cfib_unmap().
 *
 * Before cfib_swap() can be called, one must initialize a fiber for current
 * thread by calling cfib_init_thread().
 */
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

cfib_t* cfib_init_thread__prof__();
cfib_t* cfib_new__prof__(void* start_routine, void* args, uint32_t ssize);

/*! Initialize a fiber for current thread.
 *
 * This function MUST be called in a thread before cfib_swap() function can be
 * called. Prior to calling cfib_init() in a thread, calling cfib_swap() will
 * lead program exit with an error message disciplining the programmer who was
 * too proud to read the fucking documentation.
 * 
 * @return a context for this thread's main fiber.
 */
cfib_t* cfib_init_thread();

/*! Allocates a fiber and initialies it's stack.
 *
 * This function allocates a new fiber and it's stack, then initializes the 
 * stack so that fiber execution begins at 'start_routine' -function, and
 * the function receives the 'args' as it's argument.
 * 
 * The argument 'start_routine' must be a pointer to a function which has
 * the signature of void (*)(TYPE *), where TYPE is any type you desire.
 * In layman terms, the function for 'start_routine'
 * - must take only one pointer (of any type) as an argument
 * - has a return type of void
 *
 * The argument 'start_routine' should always be explicitly cast to void*
 *
 * @param[in] start_routine a pointer to a function to be executed when cfib_swap() is called on this context.
 * @param[in] args pointer to argument data passed as 1st argument to start_routine.
 * @param[in] ssize the maximum size of the stack, will be automatically rounded to page boundary.
 * @return pointer to the new fiber, or NULL if memory allocation failed.
 */
cfib_t* cfib_new(void* start_routine, void* args, uint32_t ssize);

#ifndef _CFIB_C11_H_
cfib_t* cfib_get_current__noassert__();
void cfib_swap__noassert__(cfib_t *to);

#ifndef NDEBUG
cfib_t* cfib_get_current();
#else
#define cfib_get_current cfib_get_current__noassert__
#endif

#ifndef NDEBUG
/*! Swap current fiber with the one provided as argument.
 *
 * Swaps current execution context to the one provided as an argument. That is,
 * save execution state to current context, then swap to provided context
 * and continue executing where it left off.
 *
 * @param[in/out] to the context from which execution should continue.
 *
 * @remark It is ENTIRELY up to the programmer to keep tabs on different contexts.
 */
void cfib_swap(cfib_t* to);
#else
#define cfib_swap cfib_swap__noassert__
#endif

#endif /* #ifndef _CFIB_C11_H */

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
