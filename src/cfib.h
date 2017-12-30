#ifndef _CFIB_H_
#define _CFIB_H_

/** @file cfib.h */

#include <stddef.h>
#include <stdint.h>
#include <assert.h>

/** Default stack size.
 *
 * This is the constant which defines compile time default size for all created
 * fibers. This default can be overriden by an optional argument when creating
 * or initializing a fiber.
 */
#define CFIB_DEF_STACK_SIZE (1<<16)
#define _CFIB_MGK1 (uintptr_t)0xCF1BC1B0CF1BC1B0
#define _CFIB_MGK2 (uintptr_t)0xCF1BC107CF1BC107

#ifdef __cplusplus

#if __cplusplus <= 199711L
#warning "cfib.h could not detect C++11 support. If using GCC or compatible, consider using --std=c++11"
#endif

extern "C" {
#endif

/** This is the struct that holds execution context for each fiber.
 *
 * In essence, this struct IS the fiber itself, but in documentation we might
 * refer to the instance of this struct also with words "context" or
 * "execution context".
 *
 * The data of this struct is accessed and modified by all cfib_* functions.
 *
 * To allocate and initialize a new fiber, use cfib_new().
 *
 * To switch into a fiber, use cfib_swap().
 *
 * To destroy a fiber and free it's memory, use cfib_unmap().
 *
 * Before cfib_swap() can be called, one must initialize a fiber for current
 * thread by calling cfib_init_thread().
 *
 * The reason why this struct is (at least partially) declared in the client
 * header cfib.h is because, for performance reasons, some of the library
 * functions are static inline. That is, some functions are declared and
 * implemented in this header, and thus they need access to some members
 * of this struct. At the time of writing, cfib_get_current() and
 * cfib_swap() were both such static inline functions.
 */
typedef struct {
    /** Stack pointer of a saved context.
     *
     * Just before pivoting stack to new context's stack pointer, the old
     * context's stack pointer will be saved to this stack pointer variable.
     */
    unsigned char* sp;
    /** The lower boundary of a fiber's stack window.
     *
     * The stack (pointer) can grow (down on most CPUs) up to this boundary,
     * after which system will generate SIGSEGV or similar fault.
     */
    unsigned char* stack_ceiling;
    /** The upper boundary of a fiber's stack window.
     *
     * Initially each context's stack pointer points to the stack floor.
     * But before a call to the fiber's call vector (function pointer)
     * can be made, specific data will be pushed to the stack, and thus
     * the stack pointer, in practice, never points to stack floor.
     */
    unsigned char* stack_floor;
    /** A library-internal magic variable.
     *
     * The library uses this variable to determine that the fiber was properly
     * allocated and initialized via compatible library interfaces.
     */
    uintptr_t _magic;
    /** An opaque pointer to library private data, if any.
     *
     * This is an opaque pointer to fiber-specific library internal data.
     * Client code should NEVER access this member directly, because it's
     * semantics are known only to the library.
     */
    void* _private;
} cfib_t;

/** Function signature type for fiber entrypoint.
 *
 * This is the signature type for fiber entrypoint function. It can be used
 * for typecasting a function of compatible signature. Example:
 *
 * void func(int* n);
 * int num = 42;
 * ...
 * cfib_new((cfib_func)func, (void*)&num);
 *
 */
typedef void (*cfib_func)(void*);

struct _cfib_tag {
    const char* _name;
    uintptr_t _magic;
    char __padding__[12];
};

typedef struct {
    unsigned stack_size;
    unsigned flags;
    struct _cfib_tag* (*tag)(void);
} cfib_attr_t;

#define CFIB_STKEXEC    0x00000001
#define CFIB_PREFAULT   0x00000016

struct _cfib_tls {
    cfib_t* current;
    cfib_t* previous;
};

/** @var _cfib_tls
 * @brief CFib library specific thread-local data.
 *
 * The reason we prefix this identifier with underscore is that the library
 * might change the name of this variable later. It is provided here only so
 * that the cfib_swap* functions can swap current fiber to another. The reason
 * for exposing this thread-local variable is performance. In ELF platforms
 * a dynamic library call overhead just to get one pointer is huge waste. And
 * even if static-compiled, or running in PE/COFF, we save one jump.
 *
 * To get pointer to current or previous fiber, use the cfib_get_current() or
 * cfib_get_previous() inline accessor functions. Only the source code
 * interface of the accessor function is guaranteed to be stable across
 * library versions.
 *
 * The compiler used must support either C++11 thread_local or
 * C11 _Thread_local primitives. If it supports neither, upgrade your compiler.
 * This is not an impossible task, since GCC has supported C++11 thread_local
 * since 4.8, and C11 _Thread_local since 4.9. Clang has supported both for
 * quite a long while. It is preferred not to make any workarounds, especially
 * for proprietary compilers/platforms.
 */
#ifdef __cplusplus
extern thread_local struct _cfib_tls _cfib_tls;
#else
extern _Thread_local struct _cfib_tls _cfib_tls;
#endif

/** Initialize a fiber for current thread.
 *
 * This function MUST be called in a thread before cfib_swap() function can be
 * called. Prior to calling cfib_init_thread(), calling cfib_swap() will lead 
 * to program exit with an error message disciplining the programmer who was
 * too proud to read the fucking documentation.
 * 
 * @return a context for this thread's main fiber.
 */
cfib_t* cfib_init_thread();

/** Allocates a fiber and initialies it's stack.
 *
 * This function allocates a new fiber and it's stack, then initializes the 
 * stack so that fiber execution begins at 'start_routine' -function, and
 * the function receives the 'args' as it's argument.
 * 
 * The argument 'start_routine' must point to a function which has
 * the signature of void (void *). In layman terms, the function for
 * 'start_routine'
 * - must take only pointer with type of void* as an argument
 * - does not return a value ie. has return type of void
 *
 * However, it is permissible to pass as 'start_routine' a function which has
 * argument type other than void*. If this is the case, the function pointer
 * should be explicitly cast into cfib_func. Argument 'args' should be cast to
 * void*. Example
 *
 * void my_fib_func(my_type_t* type);
 * my_type_t my_data = { ... };
 * ...
 * cfib_new((cfib_func)my_fib_func, (void*)&my_data);
 *
 * NOTE: it is important that the argument type of fiber function is a type of
 * bit-width equal or less than the bit-width of void*. Most often this type
 * would be a pointer to specific kind of struct etc. CFib does not care what
 * the argument type is, the 'args' will be passed as-is to the called function
 * as it's first argument.
 *
 * If compiling with C++, it is recommended to call cfib_new() without
 * explicit typecasting on the arguments. An inline function template is
 * provided, which makes the compiler deduce whether the provided
 * arguments are of compatible type, and if so, typecasting is done
 * automatically.
 *
 * @param[in] start_routine a pointer to a function to be executed when cfib_swap() is called on this context.
 * @param[in] args pointer to argument data passed as 1st argument to start_routine.
 * @param[in] attr attributes for this fiber, if NULL, defaults are used
 * @return pointer to the new fiber, or NULL if memory allocation failed.
 */
cfib_t* cfib_new(cfib_func start_routine, void* args, const cfib_attr_t* attr);

#ifdef __cplusplus
template <typename T>
static inline cfib_t* cfib_new(void (*start_routine)(T*), T* args, const cfib_attr_t* attr) {
    return cfib_new((cfib_fn)start_routine, (void*)args, attr);
}
#endif


static inline cfib_t* cfib_get_current() {
    assert("CALL cfib_init_thread() BEFORE CALLING cfib_get_current() !!!" && _cfib_tls.current != NULL && _cfib_tls.current->_magic == ((uintptr_t)_cfib_tls.current ^ _CFIB_MGK1));
    return _cfib_tls.current;
}

static inline cfib_t* cfib_get_current__noassert__() {
    return _cfib_tls.current;
}

static inline cfib_t* cfib_get_previous() {
    assert("CALL cfib_init_thread() BEFORE CALLING cfib_get_previous() !!!" && _cfib_tls.previous != NULL && _cfib_tls.previous->_magic == ((uintptr_t)_cfib_tls.previous ^ _CFIB_MGK1));
    return _cfib_tls.previous;
}

static inline cfib_t* cfib_get_previous__noassert__() {
    return _cfib_tls.previous;
}

/** Saves context to sp1, pivots to sp2, restores context and returns.
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
 * guarantees that the API/ABI for this function remain stable between
 * different library versions.
 */
void _cfib_swap(unsigned char** sp1, unsigned char* sp2);

/** Swap current fiber with the one provided as argument.
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
    assert("CALL cfib_init_thread() BEFORE CALLING cfib_swap() !!!" && _cfib_tls.current != NULL && _cfib_tls.current->_magic == ((uintptr_t)_cfib_tls.current ^ _CFIB_MGK1));
    assert("Argument cfib_t* to was NOT created via cfib_new() !!!" && to != NULL && to->_magic == ((uintptr_t)to ^ _CFIB_MGK1));
    _cfib_tls.previous = _cfib_tls.current;
    _cfib_tls.current = to;
    _cfib_swap(&_cfib_tls.previous->sp, to->sp);
}

/** Swap current fiber with the one provided as argument (faster).
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
    _cfib_tls.previous = _cfib_tls.current;
    _cfib_tls.current = to;
    _cfib_swap(&_cfib_tls.previous->sp, to->sp);
}

/** Unmap the stack memory of the provided context.
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


#define CFIB_TAG_DECL(identifier)\
struct _cfib_tag* identifier();

#define CFIB_TAG_CTOR(identifier)\
struct _cfib_tag* identifier() {\
    static struct _cfib_tag ret = {\
        ._name = #identifier,\
        ._magic = (uintptr_t)0xCF1BC107CF1BC107,\
    };\
    return &ret;\
}

#ifdef __cplusplus
} /* extern "C" { */
#endif

#ifdef NDEBUG
#define cfib_get_current cfib_get_current__noassert__
#define cfib_swap cfib_swap__noassert__
#endif

#endif /* _CFIB_H_  */
