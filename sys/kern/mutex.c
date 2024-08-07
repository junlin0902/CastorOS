/*
 * Copyright (c) 2023 Ali Mashtizadeh
 * All rights reserved.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <sys/cdefs.h>
#include <sys/kassert.h>
#include <sys/kconfig.h>
#include <sys/kdebug.h>
#include <sys/kmem.h>
#include <sys/mp.h>
#include <sys/queue.h>
#include <sys/thread.h>
#include <sys/spinlock.h>
#include <sys/waitchannel.h>
#include <sys/mutex.h>
#include <errno.h>

/*
 * For debugging so we can assert the owner without holding a reference to the 
 * thread.  You can access the current thread through curProc[CPU()].
 */
extern Thread *curProc[MAX_CPUS];

void
Mutex_Init(Mutex *mtx, const char *name)
{
    Spinlock_Init(&mtx->lock, name, SPINLOCK_TYPE_NORMAL);
    WaitChannel_Init(&mtx->chan, name);

    return;
}

void
Mutex_Destroy(Mutex *mtx)
{
    WaitChannel_Destroy(&mtx->chan);
    Spinlock_Destroy(&mtx->lock);
    return;
}

/**
 * Mutex_Lock --
 *
 * Acquires the mutex.
 */
void
Mutex_Lock(Mutex *mtx)
{
    /*
     * You cannot hold a spinlock while trying to acquire a Mutex that may 
     * sleep!
     */
    ASSERT(Critical_Level() == 0);

    /* XXXFILLMEIN */
    Spinlock_Lock(&mtx->lock);
    while (mtx->status == MTX_STATUS_LOCKED) {
        WaitChannel_Lock(&mtx->chan);
        Spinlock_Unlock(&mtx->lock);
        WaitChannel_Sleep(&mtx->chan);
        Spinlock_Lock(&mtx->lock);
    }
    mtx->status = MTX_STATUS_LOCKED;
    mtx->owner = curProc[CPU()];
    Spinlock_Unlock(&mtx->lock);
}

/**
 * Mutex_TryLock --
 *
 * Attempts to acquire the user mutex.  Returns EBUSY if the lock is already 
 * taken, otherwise 0 on success.
 */
int
Mutex_TryLock(Mutex *mtx)
{
    /* XXXFILLMEIN */
    if (mtx->status == MTX_STATUS_LOCKED) {
        return EBUSY;
    }
    mtx->status = MTX_STATUS_LOCKED;
    mtx->owner = curProc[CPU()];    

    return 0;
}

/**
 * Mutex_Unlock --
 *
 * Releases the user mutex.
 */
void
Mutex_Unlock(Mutex *mtx)
{
    /* XXXFILLMEIN */
    Spinlock_Lock(&mtx->lock);
    mtx->status = MTX_STATUS_UNLOCKED;
    mtx->owner = NULL;
    WaitChannel_Wake(&mtx->chan);
    Spinlock_Unlock(&mtx->lock);

    return;
}

