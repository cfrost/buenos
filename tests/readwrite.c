#include "tests/lib.h"
#include "proc/syscall.h"

int main(void)
{
    char buffer[3] = "hej";
    syscall_write(FILEHANDLE_STDOUT,buffer,3);
    
    syscall_halt();

    return 0;
}
