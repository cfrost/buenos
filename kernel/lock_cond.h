/* 
 * File:   lock_cond.h
 * Author: Christoffer Frost
 *
 * Created on 25. februar 2013, 13:02
 */

#ifndef LOCK_COND_H
#define	LOCK_COND_H

#include "kernel/spinlock.h"
#include "kernel/thread.h"


// kun en struct med eller uden atributter ???
typedef struct {
	int lock;
	spinlock_t spinlock;
} lock_t;

// lock functions
int lock_reset(lock_t *lock);
void lock_acquire(lock_t *lock);
void lock_release(lock_t *lock);

//COND

typedef int cond_t;

void condition_init(cond_t *cond);
void condition_wait(cond_t *cond, lock_t *lock);
void condition_signal(cond_t *cond, lock_t *lock);
void condition_broadcast(cond_t *cond, lock_t *lock);

#endif	/* LOCK_COND_H */

