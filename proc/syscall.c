/*
 * System calls.
 *
 * Copyright (C) 2003 Juha Aatrokoski, Timo Lilja,
 *   Leena Salmela, Teemu Takanen, Aleksi Virtanen.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: syscall.c,v 1.3 2004/01/13 11:10:05 ttakanen Exp $
 *
 */
#include "kernel/cswitch.h"
#include "proc/syscall.h"
#include "kernel/halt.h"
#include "kernel/panic.h"
#include "lib/libc.h"
#include "kernel/assert.h"
#include "proc/process.h"
#include "drivers/device.h"
#include "drivers/gcd.h"
#include "fs/vfs.h"
#include "vm/vm.h"
#include "vm/pagepool.h"

void syscall_exit(int retval) {
    process_finish(retval);
}

int syscall_write(uint32_t fd, char *s, int len) {
    gcd_t *gcd;
    device_t *dev;
    if (fd == FILEHANDLE_STDOUT || fd == FILEHANDLE_STDERR) {
        dev = device_get(YAMS_TYPECODE_TTY, 0);
        gcd = (gcd_t *) dev->generic_device;
        return gcd->write(gcd, s, len);
    } else {
        if (process_check_file(fd)) {
            return vfs_write(fd, s, len);
        } else {
            return -1;
        }
    }
}

int syscall_read(uint32_t fd, char *s, int len) {
    gcd_t *gcd;
    device_t *dev;
    if (fd == FILEHANDLE_STDIN) {
        dev = device_get(YAMS_TYPECODE_TTY, 0);
        gcd = (gcd_t *) dev->generic_device;
        return gcd->read(gcd, s, len);
    } else {
        if (process_check_file(fd)) {
            return vfs_read(fd, s, len);
        } else {
            return -1;
        }
    }
}

int syscall_join(process_id_t pid) {
    return process_join(pid);
}

process_id_t syscall_exec(const char *filename) {
    return process_spawn(filename);
}

//vm_map bed om mere hukommelse
//vm_unmap anti/release more mem

int syscall_memlimit(uint32_t heap_end) {
    uint32_t current_end = process_get_current_process_entry()->heap_end;
    // return current heap end
    if (heap_end == (int) NULL) {
        return current_end;
    }
    // free - vm_unmap not implementated
    if (heap_end < current_end) {
        return (int) NULL;
    }

    //check med stack_end

    // fundet i process.c
    int i;
    for (i = current_end; current_end <= heap_end; i++) {
        uint32_t phys_page = pagepool_get_phys_page();
        KERNEL_ASSERT(phys_page != 0);
        pagetable_t pt = thread_get_current_thread_entry()->pagetable;
        KERNEL_ASSERT(pt != NULL);
        vm_map(pt, phys_page, i, 1);
        //heap_end += pagesize;
    }
    process_get_current_process_entry()->heap_end = heap_end;
    return heap_end;

}

openfile_t syscall_open(const char *filename) {
    openfile_t fid = vfs_open(filename);
    KERNEL_ASSERT(fid > 2 | fid < 0);
    // process file table is full
    if (process_add_file(fid) < 0) return -10;
    return fid;
}

int syscall_close(openfile_t filehandle) {
    process_rem_file(filehandle);
    return vfs_close(filehandle);
}

int syscall_create(const char *pathname, int size) {
    return vfs_create(pathname, size);
}

int syscall_delete(const char *pathname) {
    return vfs_remove(pathname);
}

int syscall_seek(openfile_t filehandle, int offset) {
    // Check offset is within boundries aka file size
    return vfs_seek(filehandle, offset)
}

/**
 * Handle system calls. Interrupts are enabled when this function is
 * called.
 *
 * @param user_context The userland context (CPU registers as they
 * where when system call instruction was called in userland)
 */
void syscall_handle(context_t *user_context) {
    int A1 = user_context->cpu_regs[MIPS_REGISTER_A1];
    int A2 = user_context->cpu_regs[MIPS_REGISTER_A2];
    int A3 = user_context->cpu_regs[MIPS_REGISTER_A3];
    /* When a syscall is executed in userland, register a0 contains
     * the number of the syscall. Registers a1, a2 and a3 contain the
     * arguments of the syscall. The userland code expects that after
     * returning from the syscall instruction the return value of the
     * syscall is found in register v0. Before entering this function
     * the userland context has been saved to user_context and after
     * returning from this function the userland context will be
     * restored from user_context.
     */
    switch (user_context->cpu_regs[MIPS_REGISTER_A0]) {
        case SYSCALL_HALT:
            halt_kernel();
            break;
        case SYSCALL_EXIT:
            syscall_exit(A1);
            break;
        case SYSCALL_WRITE:
            user_context->cpu_regs[MIPS_REGISTER_V0] =
                    syscall_write(A1, (char *) A2, A3);
            break;
        case SYSCALL_READ:
            user_context->cpu_regs[MIPS_REGISTER_V0] =
                    syscall_read(A1, (char *) A2, A3);
            break;
        case SYSCALL_JOIN:
            user_context->cpu_regs[MIPS_REGISTER_V0] =
                    syscall_join(A1);
            break;
        case SYSCALL_EXEC:
            user_context->cpu_regs[MIPS_REGISTER_V0] =
                    syscall_exec((char *) A1);
            break;
        case SYSCALL_MEMLIMIT:
            user_context->cpu_regs[MIPS_REGISTER_V0] =
                    syscall_memlimit((uint32_t) A1);
            break;
        case SYSCALL_OPEN:
            user_context->cpu_regs[MIPS_REGISTER_V0] =
                    syscall_open((char *) A1);
            break;
        case SYSCALL_CLOSE:
            user_context->cpu_regs[MIPS_REGISTER_V0] =
                    syscall_close((openfile_t) A1);
            break;
        case SYSCALL_CREATE:
            user_context->cpu_regs[MIPS_REGISTER_V0] =
                    syscall_create((char *) A1, A2);
            break;
        case SYSCALL_DELETE:
            user_context->cpu_regs[MIPS_REGISTER_V0] =
                    syscall_delete((char *) A1);
            break;
        case SYSCALL_SEEK:
            user_context->cpu_regs[MIPS_REGISTER_V0] =
                    syscall_seek(A1, A2);
            break;

        default:
            KERNEL_PANIC("Unhandled system call\n");
    }

    /* Move to next instruction after system call */
    user_context->pc += 4;
}
