#ifndef _CFIB_C11_H_
#define _CFIB_C11_H_

#include "cfib.h"

#ifdef __cplusplus
extern "C" {
#endif

extern _Thread_local cfib_t* _cfib_current;

inline static cfib_t* cfib_get_current() {
#ifndef NDEBUG
    assert("CALL cfib_init_thread() BEFORE CALLING cfib_get_current() !!!" && _cfib_current != NULL);
#endif
    return _cfib_current;
}

/** @internal
 *
 * !!! Do not call this function directly; use cfib_swap() instead !!!
 *
 * This function is implemented in a system and CPU specific assembler file.
 * The correct assembler file for a specific system+CPU combo is compiled and
 * linked into the library by the library build script.
 *
 */
void _cfib_swap(unsigned char** sp1, unsigned char* sp2);

inline static void cfib_swap(cfib_t* to) {
#ifndef NDEBUG
    assert("CALL cfib_init_thread() BEFORE CALLING cfib_swap() !!!" && _cfib_current != NULL);
#endif
    cfib_t* from = _cfib_current;
    _cfib_current = to;
    _cfib_swap(&from->sp, to->sp);
}

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* _CFIB_H_  */
