#include "tests/lib.h"

static const char prog[] = "[arkimedes]proc_test"; /* The program to start. */

int main(void) {
    uint32_t child;
    printf("Beginning proc_test \n");
    child = syscall_exec(prog);
    syscall_join(child);
    printf("Ending proc_test \n");
    syscall_halt();
    return 0;
}
