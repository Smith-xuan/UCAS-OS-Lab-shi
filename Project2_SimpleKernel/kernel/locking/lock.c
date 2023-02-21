#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>

mutex_lock_t mlocks[LOCK_NUM];

void init_locks(void)
{
    /* TODO: [p2-task2] initialize mlocks */
    for(int i = 0; i < LOCK_NUM; i++){
        mlocks[i].lock.status = UNLOCKED;
        init_list_head(&(mlocks[i].block_queue));
    }
}

void spin_lock_init(spin_lock_t *lock)
{
    /* TODO: [p2-task2] initialize spin lock */
}

int spin_lock_try_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] try to acquire spin lock */
    return 0;
}

void spin_lock_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] acquire spin lock */
}

void spin_lock_release(spin_lock_t *lock)
{
    /* TODO: [p2-task2] release spin lock */
}

int do_mutex_lock_init(int key)
{
    /* TODO: [p2-task2] initialize mutex lock */
    int mlock_idx = key % 16;
    return mlock_idx;
}

void do_mutex_lock_acquire(int mlock_idx)
{
    /* TODO: [p2-task2] acquire mutex lock */
    //if the lock has been gotten, do block the next_running
    if(mlocks[mlock_idx].lock.status == LOCKED){
        do_block(&(next_running->list), &(mlocks[mlock_idx].block_queue));
    }
    else{ // else set the lock locked
        mlocks[mlock_idx].lock.status = LOCKED;
    }
    
}

void do_mutex_lock_release(int mlock_idx)
{
    /* TODO: [p2-task2] release mutex lock */
    //if block_list is empty, just set the lock unlocked
    if(list_empty(&(mlocks[mlock_idx].block_queue))){
        mlocks[mlock_idx].lock.status = UNLOCKED;
    }
    else{ //else do_unblock the block_queue
        do_unblock(&(mlocks[mlock_idx].block_queue));
    }
}
