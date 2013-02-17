#include "tests/lib.h"
#include "proc/syscall.h"
#include "lib/debug.h"
#include "lib/libc.h"

int main(void) {
    char *buffer;
    char *buffer2;
    int len;

    /*
        len = snprintf(buffer, 63, "Hello user! Press any key.\n");
        syscall_write(FILEHANDLE_STDOUT, buffer, len);

        len = syscall_read(FILEHANDLE_STDIN, buffer2, 63);
        buffer2[len] = '\0';

        len = snprintf(buffer, 63, "You said: '%s'\n", buffer2);

        syscall_write(FILEHANDLE_STDOUT, buffer, len);
     */

    buffer = "hej\n";
    syscall_write(FILEHANDLE_STDOUT, buffer, 64);
    buffer2 = "";
    len = syscall_read(FILEHANDLE_STDIN, buffer2, 64);
    buffer2[len] = '\0';

    syscall_write(FILEHANDLE_STDOUT, buffer2, len);

    syscall_halt();

    return 0;
}
