#include "tests/lib.h"

static const size_t BUFFER_SIZE = 20;
static const char prog[] = "[arkimedes]proc_test";

int main(void) {
    int ret;
    uint32_t child;
    char buffer[BUFFER_SIZE];
    heap_init();
    puts("Press c for create and q for kill \n");
    while (1) {
        readline(buffer, BUFFER_SIZE);
        if (strcmp(buffer, "q") == 0) {
            puts("Process terminated \n");
            syscall_exit(child);
        } else if (strcmp(buffer, "c") == 0) {
            printf("Starting new process \n");
            child = syscall_exec(prog);
            printf("Joining child %d\n", child);
            ret = syscall_join(child);
            printf("Child joined with status: %d\n", ret);
        } else {
            printf("Bad input. \n");
        }
    }
    syscall_halt();
    return 0;
}
