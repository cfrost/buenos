#include "tests/lib.h"
#include "proc/syscall.h"

int main(void)
{
    char buffer[19] = "WRITE IS WORKING \n";
    syscall_write(FILEHANDLE_STDOUT,buffer,19);
    
    syscall_halt();

    return 0;
}
