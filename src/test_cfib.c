#include <stdio.h>
#define NDEBUG
#if _HAVE_C11_THREAD_LOCAL
#include "cfib_C11.h"
#else
#include "cfib.h"
#endif

cfib_t *main_context;

void test_fiber(void *arg) {
    printf("test_fiber 1\n");
    cfib_swap(main_context);
    printf("test_fiber 2\n");
}

int main(int argc, char** argv) {
    // NOTE: Before any calls to cfib_* functions can be made, 
    // cfib_init_thread() MUST be called at least ONCE in some thread!
    // 
    // If this advice is NOT heeded, libcfib will crash the program on
    // an assertion with a condoning message.
    main_context = cfib_init_thread();
    printf("current after init: %p\n", cfib_get_current());
    cfib_t* test_context = cfib_new((void*)test_fiber, NULL, 0);
    printf("main 1\n");
    cfib_swap(test_context);
    printf("main 2\n");
    cfib_swap(test_context);
    return 0;
}
