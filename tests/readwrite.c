#include "tests/lib.h"
#include "proc/syscall.h"

int main(void) {
    char c;
    
    while(1){
        syscall_read(stdin,&c,1);
        syscall_write(stdout,&c,1);
        if (c == 'q') syscall_halt(); // press 'q' to quit
    }
    
    return 0;
}
