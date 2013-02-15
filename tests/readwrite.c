#include "tests/lib.h"
#include "proc/syscall.h"

int main(void)
{
    char buffer[17] = "WRITE IS WORKING\n";
    syscall_write(FILEHANDLE_STDOUT,buffer,17);
    
    syscall_halt();

    return 0;
}
