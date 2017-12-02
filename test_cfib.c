#include <stdio.h>
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
    main_context = cfib_init_thread();
    cfib_t* test_context = cfib_new((void*)test_fiber, NULL, 0);
    printf("main 1\n");
    cfib_swap(test_context);
    printf("main 2\n");
    cfib_swap(test_context);
    return 0;
}
