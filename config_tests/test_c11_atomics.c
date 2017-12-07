#include <stdatomic.h>

atomic_flag lock = ATOMIC_FLAG_INIT;

int main(int argc, char** argv) {
    while(atomic_flag_test_and_set_explicit(&lock, memory_order_relaxed));
    atomic_flag_clear_explicit(&lock, memory_order_release);
    return 0;
}
