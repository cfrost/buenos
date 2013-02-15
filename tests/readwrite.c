#include "tests/lib.h"
#include "proc/syscall.h"

int main(void)
{
    char buffer[35] = "------- WRITE IS WORKING --------\n";
    syscall_write(FILEHANDLE_STDOUT,buffer,35);
    
    syscall_halt();

    return 0;
}
