/*
 * Process startup.
 *
 * Copyright (C) 2003-2005 Juha Aatrokoski, Timo Lilja,
 *       Leena Salmela, Teemu Takanen, Aleksi Virtanen.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are ppermitted provided that the following conditions
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
 * $Id: process.c,v 1.11 2007/03/07 18:12:00 ttakanen Exp $
 *
 */

#include "proc/process.h"
#include "proc/elf.h"
#include "kernel/thread.h"
#include "kernel/assert.h"
#include "kernel/interrupt.h"
#include "kernel/config.h"
#include "fs/vfs.h"
#include "drivers/yams.h"
#include "vm/vm.h"
#include "vm/pagepool.h"
#include "lib/libc.h"
#include "kernel/sleepq.h"
#include "lib/debug.h"

/** @name Process startup
 *
 * This module contains facilities for managing userland process.
 */

process_control_block_t process_table[PROCESS_MAX_PROCESSES];

spinlock_t process_slock;

/* Bestem om locks skal sættes her eller når funktionen kaldes*/
void process_reset(process_control_block_t *pcb) {
    pcb->parent_id = -1;
    pcb->state = PROCESS_FREE;
    pcb->name[0] = '\0';
}

/* Find free PID in process_table, if full return -1*/
process_id_t process_find_free() {
    process_id_t pid;
    for (pid = 0; pid < PROCESS_MAX_PROCESSES; ++pid) {
        if (process_table[pid].state != PROCESS_FREE)
            continue;
        else
            return pid;
    }
    return -1;
}

process_id_t process_get_current_process(void) {
    return thread_get_current_thread_entry()->process_id;
}

process_control_block_t *process_get_current_process_entry(void) {
    return &process_table[process_get_current_process()];
}

process_control_block_t *process_get_process_entry(process_id_t pid) {
    return &process_table[pid];
}

/**
 * Starts one userland process. The thread calling this function will
 * be used to run the process and will therefore never return from
 * this function. This function asserts that no errors occur in
 * process startup (the executable file exists and is a valid ecoff
 * file, enough memory is available, file operations succeed...).
 * Therefore this function is not suitable to allow startup of
 * arbitrary processes.
 *
 * @executable The name of the executable to be run in the userland
 * process
 */
void process_start(const process_id_t pid) {
    thread_table_t *my_entry;
    pagetable_t *pagetable;
    uint32_t phys_page;
    context_t user_context;
    uint32_t stack_bottom;
    elf_info_t elf;
    openfile_t file;

    int i;

    interrupt_status_t intr_status;

    my_entry = thread_get_current_thread_entry();

    /* If the pagetable of this thread is not NULL, we are trying to
       run a userland process for a second time in the same thread.
       This is not possible. */
    KERNEL_ASSERT(my_entry->pagetable == NULL);

    pagetable = vm_create_pagetable(thread_get_current_thread());
    KERNEL_ASSERT(pagetable != NULL);

    intr_status = _interrupt_disable();
    my_entry->pagetable = pagetable;
	// Sets new threads process id to new spawned pid.
	my_entry->process_id = pid;
    _interrupt_set_state(intr_status);

    // changed
    file = vfs_open((char *) process_table[pid].name);
    /* Make sure the file existed and was a valid ELF file */
    KERNEL_ASSERT(file >= 0);
    KERNEL_ASSERT(elf_parse_header(&elf, file));

    /* Trivial and naive sanity check for entry point: */
    KERNEL_ASSERT(elf.entry_point >= PAGE_SIZE);

    /* Calculate the number of pages needed by the whole process
       (including userland stack). Since we don't have proper tlb
       handling code, all these pages must fit into TLB. */
    KERNEL_ASSERT(elf.ro_pages + elf.rw_pages + CONFIG_USERLAND_STACK_SIZE
            <= _tlb_get_maxindex() + 1);

    /* Allocate and map stack */
    for (i = 0; i < CONFIG_USERLAND_STACK_SIZE; i++) {
        phys_page = pagepool_get_phys_page();
        KERNEL_ASSERT(phys_page != 0);
        vm_map(my_entry->pagetable, phys_page,
                (USERLAND_STACK_TOP & PAGE_SIZE_MASK) - i*PAGE_SIZE, 1);
    }

    /* Allocate and map pages for the segments. We assume that
       segments begin at page boundary. (The linker script in tests
       directory creates this kind of segments) */
    for (i = 0; i < (int) elf.ro_pages; i++) {
        phys_page = pagepool_get_phys_page();
        KERNEL_ASSERT(phys_page != 0);
        vm_map(my_entry->pagetable, phys_page,
                elf.ro_vaddr + i*PAGE_SIZE, 1);
    }

    for (i = 0; i < (int) elf.rw_pages; i++) {
        phys_page = pagepool_get_phys_page();
        KERNEL_ASSERT(phys_page != 0);
        vm_map(my_entry->pagetable, phys_page,
                elf.rw_vaddr + i*PAGE_SIZE, 1);
    }

    /* Put the mapped pages into TLB. Here we again assume that the
       pages fit into the TLB. After writing proper TLB exception
       handling this call should be skipped. */
    intr_status = _interrupt_disable();
    tlb_fill(my_entry->pagetable);
    _interrupt_set_state(intr_status);

    /* Now we may use the virtual addresses of the segments. */

    /* Zero the pages. */
    memoryset((void *) elf.ro_vaddr, 0, elf.ro_pages * PAGE_SIZE);
    memoryset((void *) elf.rw_vaddr, 0, elf.rw_pages * PAGE_SIZE);

    stack_bottom = (USERLAND_STACK_TOP & PAGE_SIZE_MASK) -
            (CONFIG_USERLAND_STACK_SIZE - 1) * PAGE_SIZE;
    memoryset((void *) stack_bottom, 0, CONFIG_USERLAND_STACK_SIZE * PAGE_SIZE);

    /* Copy segments */

    if (elf.ro_size > 0) {
        /* Make sure that the segment is in proper place. */
        KERNEL_ASSERT(elf.ro_vaddr >= PAGE_SIZE);
        KERNEL_ASSERT(vfs_seek(file, elf.ro_location) == VFS_OK);
        KERNEL_ASSERT(vfs_read(file, (void *) elf.ro_vaddr, elf.ro_size)
                == (int) elf.ro_size);
    }

    if (elf.rw_size > 0) {
        /* Make sure that the segment is in proper place. */
        KERNEL_ASSERT(elf.rw_vaddr >= PAGE_SIZE);
        KERNEL_ASSERT(vfs_seek(file, elf.rw_location) == VFS_OK);
        KERNEL_ASSERT(vfs_read(file, (void *) elf.rw_vaddr, elf.rw_size)
                == (int) elf.rw_size);
    }


    /* Set the dirty bit to zero (read-only) on read-only pages. */
    for (i = 0; i < (int) elf.ro_pages; i++) {
        vm_set_dirty(my_entry->pagetable, elf.ro_vaddr + i*PAGE_SIZE, 0);
    }

    /* Insert page mappings again to TLB to take read-only bits into use */
    intr_status = _interrupt_disable();
    tlb_fill(my_entry->pagetable);
    _interrupt_set_state(intr_status);

    /* Initialize the user context. (Status register is handled by
       thread_goto_userland) */
    memoryset(&user_context, 0, sizeof (user_context));
    user_context.cpu_regs[MIPS_REGISTER_SP] = USERLAND_STACK_TOP;
    user_context.pc = elf.entry_point;

    thread_goto_userland(&user_context);

    KERNEL_PANIC("thread_goto_userland failed.");
}

void process_init() {
    process_id_t pid;
	
	// Ensures that no spinlock is present 
    spinlock_reset(&process_slock);

	// Initialize/reset all process in the process table
    for ( pid = 0; pid < PROCESS_MAX_PROCESSES; pid++) {
        process_reset(&process_table[pid]);
    }
	DEBUG( "debug_process", "Init done\n"); 
}

process_id_t process_spawn(const char *executable) {
	DEBUG( "debug_process", "Spawn entered\n"); 
    process_id_t pid;

	/* Disable interrupts and acquiere spinlock before handeling changes 
	 * in process table */
    interrupt_status_t intr_status = _interrupt_disable();
    spinlock_acquire(&process_slock);
	
	// Find free process in table, return pid or -1 if table is full
    pid = process_find_free();
	
	DEBUG( "debug_process", "Free pid : %d\n", pid); 

    // Check if table is full
    if (pid < 0) {
        spinlock_release(&process_slock);
        _interrupt_set_state(intr_status);
		KERNEL_ASSERT(pid >= 0);
        return pid;
    }

    // Set current process state to running.
    process_table[pid].state = PROCESS_RUNNING;
	// disable spinlock

    stringcopy(process_table[pid].name, executable, PROCESS_MAX_NAMESIZE);
	process_table[pid].parent_id = process_get_current_process();	
	
    //Spinlock release . DONT!
    spinlock_release(&process_slock);
    _interrupt_set_state(intr_status);

    // Create thread that starts process_start withe PID as argument and run it
    TID_t newthread = thread_create((void (*)(uint32_t)) &process_start, pid);
	DEBUG( "debug_process", "Thread id : %d\n", newthread); 
	
    thread_run(newthread);
	DEBUG( "debug_process", "Thread running\n"); 
    return pid;
}

/* Stop the process and the thread it runs in. Sets the return value as well */
void process_finish(int retval) {
    thread_table_t *thr = thread_get_current_thread_entry();
    process_id_t current = process_get_current_process();

	// Implemented according to BUENOS roadmaps psudocode on sleepqueue at 5.2	
	
	/* Disable interrupts and acquiere spinlock before handeling changes 
	 * in process table */ 
	interrupt_status_t intr_status = _interrupt_disable();
    spinlock_acquire(&process_slock);
	
    // Store retval in current pcb and set ZOMBIE
    process_table[current].retval = retval;
    process_table[current].state = PROCESS_ZOMBIE;
	
	// Necessary code provided by the assignment description
	vm_destroy_pagetable(thr->pagetable);
    thr->pagetable = NULL;
	
	// Process done and no need for a sleep queue.
    sleepq_wake_all(&process_table[current]);
	
	// Release spinlock and enable interrupts
	spinlock_release(&process_slock);
	_interrupt_set_state(intr_status);

    thread_finish();

    // if parrent : kill ?? kill all children??
}

int process_join(process_id_t pid) {
    int retval;

	
	/* Check if argument pid is in between 0 to max processes and check 
	 * if current process is pids parrent. If such, return -1 / ERROR */
    if ((pid < 0 || pid > PROCESS_MAX_PROCESSES) ||
            process_table[pid].parent_id != process_get_current_process()) {
        return -1;
    }

	// Implemented according to BUENOS roadmaps psudocode on sleepqueue at 5.2
	
	/* Disable interrupts and acquiere spinlock before handeling changes 
	 * in process table */ 
    interrupt_status_t intr_status = _interrupt_disable();
    spinlock_acquire(&process_slock);
    // Waiting for pid to call finish
    while (process_table[pid].state != PROCESS_ZOMBIE) {
		// add 
        sleepq_add(&process_table[pid]);
        spinlock_release(&process_slock);
        thread_switch();
        spinlock_acquire(&process_slock);
    }
    // Storing return value and resets the process pid 
    retval = process_table[pid].retval;
    process_reset(&process_table[pid]);

    // Release spinlock and enable interrupts
    spinlock_release(&process_slock);
    _interrupt_set_state(intr_status);

    return retval;
}

/** @} */
