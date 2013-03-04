#include "lib/libc.h"
#include "lock_cond.h"
#include "spinlock.h"
#include "sleepq.h"

/* Initialize an already allocated lock t structure such that it can be acquired 
 * and released afterwards. The function should return 0 on success and a 
 * negative number on failure.*/
int lock_reset(lock_t *lock) {
    // lock is nonexisting
    if (lock == NULL) {
        return -1;
    }
    //reset value in lock_t struck
    spinlock_reset(&(lock->spinlock));
    lock->lock = 0;
    return 0;
}

/* Acquire the lock. A simple solution could use busy-waiting, but this is 
 * ineffcient. In your solution, you should use the sleep queue to let 
 * kernel threads wait.*/
void lock_acquire(lock_t *lock) {
    /* mark the lock as owned by the current thread; if some other thread 
     * already owns the lock then first wait until the lock is free. Lock 
     * typically includes a queue to keep track of multiple waiting threads. */

    // sleep queues

    interrupt_status_t intr_status = _interrupt_disable();
    spinlock_acquire(&(lock->spinlock));
    while (lock->lock != 0) {
        sleepq_add(lock);
        spinlock_release(&(lock->spinlock));
        // Sleepq does not sleep thread thererfore thread_switch
        thread_switch();
        spinlock_acquire(&(lock->spinlock));
    }

    lock->lock = 1;

    // Release spinlock and enable interrupts
    spinlock_release(&(lock->spinlock));
    _interrupt_set_state(intr_status);
}

void lock_release(lock_t *lock) {
    /* Disable interrupts and acquiere spinlock before handeling changes 
     * in process table */
    interrupt_status_t intr_status = _interrupt_disable();
    spinlock_acquire(&(lock->spinlock));

    lock->lock = 0;

    // Process done and no need for a sleep queue.
    sleepq_wake(lock);

    // Release spinlock and enable interrupts
    spinlock_release(lock->spinlock);
    _interrupt_set_state(intr_status);
}

void condition_init(cond_t *cond) {
    //For the lulzzzz !??!?
    // Random init value assigned
    *cond = 1;
}

void condition_wait(cond_t *cond, lock_t *lock) {
    /* release lock, put thread to sleep until condition is signaled; when 
     * thread wakes up again, reacquire lock before returning. */
    lock_release(lock);
    sleepq_add(cond);
    // Sleepq does not sleep thread thererfore thread_switch
    thread_switch();
    lock_acquire(lock);
}

void condition_signal(cond_t *cond, lock_t *lock) {
    /* if any threads are waiting on condition, wake up one of them. Caller must 
     * hold lock, which must be the same as the lock used in the wait call.*/

    /* after signal, signaling thread keeps lock, waking thread goes on the 
     * queue waiting for the lock.*/
    //lock_acquire(lock);
    sleepq_wake(cond);

}

void condition_broadcast(cond_t *cond, lock_t *lock) {
    /*  same as signal, except wake up all waiting threads.*/
    sleepq_wake_all(cond);

}