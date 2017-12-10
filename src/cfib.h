#ifndef _CFIB_H_
#define _CFIB_H_

#include <stddef.h>
#include <stdint.h>
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

/*! An internal, thread-local variable pointing to currently run fiber.
 *
 * The reason we prefix this identifier with underscore is that the library
 * might change the name of this variable later. It is provided here only so
 * that the cfib_swap* functions can swap current fiber to another. The reason
 * for exposing this thread-local variable is performance. In ELF platforms
 * a dynamic library call overhead just to get one pointer is huge waste. And
 * even if static-compiled, or running in PE/COFF, we save one jump.
 *
 * To get pointer to current fiber, use the cfib_get_current() inline accessor
 * function. Only the source code interface of the accessor function is
 * guaranteed to be stable across library versions.
 *
 * The compiler used must support either C++11 thread_local or
 * C11 _Thread_local primitives. If it supports neither, upgrade your compiler.
 * This is not an impossible task, since GCC has supported C++11 thread_local
 * since 4.8, and C11 _Thread_local since 4.9. Clang has supported both for
 * quite a long while. It is preferred not to make any workarounds, especially
 * for proprietary compilers/platforms.
 */
#ifdef __cplusplus
extern thread_local cfib_t* _cfib_current;
#else
extern _Thread_local cfib_t* _cfib_current;
#endif

/*! Initialize a fiber for current thread.
 *
 * This function MUST be called in a thread before cfib_swap() function can be
 * called. Prior to calling cfib_init_thread(), calling cfib_swap() will lead 
 * to program exit with an error message disciplining the programmer who was
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

static inline cfib_t* cfib_get_current() {
    assert("CALL cfib_init_thread() BEFORE CALLING cfib_get_current() !!!" && _cfib_current != NULL);
    return _cfib_current;
}

static inline cfib_t* cfib_get_current__noassert__() {
    return _cfib_current;
}

/*! Saves context to sp1, pivots to sp2, restores context and returns.
 *
 * This function is implemented in a platform specific assembler file. The
 * arguments sp1 and sp2 are stack pointers of current and next context.
 * By convention, caller saves all caller-saved data, then calls into this
 * function, Then this function pushes all callee-saved data, updating sp1
 * with the current sp register value, then pivots into sp2, restores all
 * callee-saved data and returns.
 *
 * Currently only cfib_swap() function calls this function directly. Nothing
 * prevents client code calling this function, except that there are no
 * guarantees that the API/ABI for this function is stable between
 * different library versions.
 */
void _cfib_swap(unsigned char** sp1, unsigned char* sp2);

/*! Swap current fiber with the one provided as argument.
 *
 * Swaps current fiber to the one provided as an argument. That is, save
 * execution state to current fiber, then swap to provided fiber in and
 * continue executing where it left off.
 *
 * @param[in/out] to the context from which execution should continue.
 *
 * @remark It is ENTIRELY up to the programmer to keep tabs on different contexts.
 */
static inline void cfib_swap(cfib_t* to) {
    assert("CALL cfib_init_thread() BEFORE CALLING cfib_swap() !!!" && _cfib_current != NULL);
    cfib_t* from = _cfib_current;
    _cfib_current = to;
    _cfib_swap(&from->sp, to->sp);
}

/*! Swap current fiber with the one provided as argument (faster).
 *
 * Swaps current fiber to the one provided as an argument. That is, save
 * execution state to current fiber, then swap to provided fiber in and
 * continue executing where it left off.
 *
 * Exactly same as cfib_swap(), but without safety assertions, which makes
 * this version faster, but also more prone to programmer error.
 *
 * When you define NDEBUG before including cfib.h, all references to
 * cfib_swap will be replaced with cfib_swap__noassert__ by the pre-processor.
 *
 * @param[in/out] to the context from which execution should continue.
 *
 * @remark It is ENTIRELY up to the programmer to keep tabs on different contexts.
 */
static inline void cfib_swap__noassert__(cfib_t *to) {
    cfib_t* from = _cfib_current;
    _cfib_current = to;
    _cfib_swap(&from->sp, to->sp);
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

#ifdef NDEBUG
#define cfib_get_current cfib_get_current__noassert__
#define cfib_swap cfib_swap__noassert__
#endif

#endif /* _CFIB_H_  */
