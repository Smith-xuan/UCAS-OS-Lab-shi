#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>
#include <os/string.h>
#include <os/smp.h>

mutex_lock_t mlocks[LOCK_NUM];
barrier_t barr[BARRIER_NUM];
condition_t cond[CONDITION_NUM];
mailbox_t mbox[MBOX_NUM];
//we promise the first 10th is for user, others for kernel

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
    int mlock_idx = key % 10;
    return mlock_idx;
}

void do_mutex_lock_acquire(int mlock_idx)
{
    uint64_t cpu_id = get_current_cpu_id();
    /* TODO: [p2-task2] acquire mutex lock */
    //if the lock has been gotten, do block the next_running[cpu_id]
    if(mlocks[mlock_idx].lock.status == LOCKED){
        do_block(&(next_running[cpu_id]->list), &(mlocks[mlock_idx].block_queue));
    }
    else{ // else set the lock locked
        mlocks[mlock_idx].lock.status = LOCKED;
        next_running[cpu_id]->lock[next_running[cpu_id]->lock_num] = &mlocks[mlock_idx];
        next_running[cpu_id]->lock_num++;
    }
    
}

void do_mutex_lock_release(int mlock_idx)
{
    uint64_t cpu_id = get_current_cpu_id();
    /* TODO: [p2-task2] release mutex lock */
    for (int i = 0; i < next_running[cpu_id]->lock_num; i++){
        if(next_running[cpu_id]->lock[i] == &mlocks[mlock_idx]){
            for(int k = i + 1; k < next_running[cpu_id] -> lock_num; k++){
                next_running[cpu_id] -> lock[k-1] = next_running[cpu_id] -> lock[k];      //We move our lock FORWARD and EXEC
            }
            next_running[cpu_id]->lock_num--;
            break;
        }
    }
    

    //if block_list is empty, just set the lock unlocked
    if(list_empty(&(mlocks[mlock_idx].block_queue))){
        mlocks[mlock_idx].lock.status = UNLOCKED;
    }
    else{ //else do_unblock the block_queue
        do_unblock(&(mlocks[mlock_idx].block_queue));
    }
}


void init_barriers(void)
{
    for (int i = 0; i < BARRIER_NUM; i++){
        barr[i].goal_num = 0;
        barr[i].now_num = 0;
        init_list_head(&(barr[i].block_queue));
    }
}

int do_barrier_init(int key, int goal)
{
    int barr_idx = key % 10;
    barr[barr_idx].goal_num = goal;
    return barr_idx;
}

void do_barrier_wait(int bar_idx)
{
    uint64_t cpu_id = get_current_cpu_id();
    barr[bar_idx].now_num++;
    if(barr[bar_idx].now_num < barr[bar_idx].goal_num){
        do_block(&(next_running[cpu_id]->list),&(barr[bar_idx].block_queue));
    }
    else{
        while(!list_empty(&(barr[bar_idx].block_queue))){
            do_unblock(&(barr[bar_idx].block_queue));
        }
        barr[bar_idx].now_num = 0;
    }
}

void do_barrier_destroy(int bar_idx)
{
    barr[bar_idx].now_num = 0;
    barr[bar_idx].goal_num = 0;
    while(!list_empty(&(barr[bar_idx].block_queue))){
        do_unblock(&(barr[bar_idx].block_queue));
    }
}

void init_conditions(void)
{
    for (int i = 0; i < CONDITION_NUM; i++){
        init_list_head(&(cond[i].block_queue));
        cond[i].status = 0;
    }
    
}

int do_condition_init(int key)
{
    int cond_idx = key % 10;
    cond[cond_idx].status = 1;
    cond[cond_idx].block_count = 0;
    return cond_idx;
}

void do_condition_wait(int cond_idx, int mutex_idx)
{
    uint64_t cpu_id = get_current_cpu_id();
    do_mutex_lock_release(mutex_idx);

    do_block(&(next_running[cpu_id]->list), &(cond[cond_idx].block_queue));

    do_mutex_lock_acquire(mutex_idx);
}

void do_condition_signal(int cond_idx)
{
    do_unblock(&(cond[cond_idx].block_queue));
}

void do_condition_broadcast(int cond_idx)
{
    while(!list_empty(&(cond[cond_idx].block_queue))){
        do_unblock(&(cond[cond_idx].block_queue));
    }
}

void do_condition_destroy(int cond_idx)
{
    while(!list_empty(&(cond[cond_idx].block_queue))){
        do_unblock(&(cond[cond_idx].block_queue));
    }
    cond[cond_idx].status = 0;
}

void init_mbox()
{
    for (int i = 0; i < MBOX_NUM; i++){
        mbox[i].status = 0;
        mbox[i].start = 0;
        mbox[i].end = 0;
        mbox[i].buf_used = 0;
    }
    
}

int do_mbox_open(char *name)
{
    int i = 0;

    for (i = 0; i < MBOX_NUM; i++){
        if(!strcmp(name, mbox[i].name)){
            return i;
        }
    }

    for (i = 0; i < MBOX_NUM; i++){
        if(mbox[i].status != 1){
            mbox[i].status = 1;
            strcpy(mbox[i].name, name);

            mbox[i].buf_used = 0;
            mbox[i].start = 0;
            mbox[i].end = 0;
            
            for(int m = 10; m < LOCK_NUM; m++){
                if(mlocks[m].lock.status == UNLOCKED){
                    mlocks[m].lock.status = LOCKED;
                    mbox[i].mutex_lock = m; // may 
                    break;
                }
            }

            for (int n = 10; n < CONDITION_NUM; n++){
                if(cond[n].status == 0){
                    cond[n].status = 1;
                    mbox[i].cond_not_empty = n;
                    break;
                }
            }

            for (int k = 10; k < CONDITION_NUM; k++){
                if(cond[k].status == 0){
                    cond[k].status = 1;
                    mbox[i].cond_not_full = k;
                    break;
                }
            }
            return i;
        }
    }
    
    return -1;
}

void do_mbox_close(int mbox_idx)
{
    mbox[mbox_idx].status = 0;
    mbox[mbox_idx].buf_used = 0;
    mbox[mbox_idx].start = 0;
    mbox[mbox_idx].end = 0;
    memset(mbox[mbox_idx].name, 0, MAX_MBOX_NAME);

    cond[mbox[mbox_idx].cond_not_empty].block_count = 0;
    cond[mbox[mbox_idx].cond_not_full].block_count = 0;
}

int do_mbox_send(int mbox_idx, void * msg, int msg_length)
{
    mlocks[mbox[mbox_idx].mutex_lock].lock.status = UNLOCKED;

    do_mutex_lock_acquire(mbox[mbox_idx].mutex_lock);
    while(mbox[mbox_idx].buf_used + msg_length > MAX_MBOX_LENGTH){
        do_condition_wait(mbox[mbox_idx].cond_not_full, mbox[mbox_idx].mutex_lock);
        cond[mbox[mbox_idx].cond_not_full].block_count++;
    }
    mbox[mbox_idx].buf_used += msg_length;

    uint8_t* src = msg;
    uint8_t* dst = mbox[mbox_idx].msg;

    for (int i = 0; i < msg_length; i++){
        dst[(mbox[mbox_idx].end + i) % MAX_MBOX_LENGTH] = src[i];
    }
    mbox[mbox_idx].end = (mbox[mbox_idx].end + msg_length) % MAX_MBOX_LENGTH;

    do_condition_broadcast(mbox[mbox_idx].cond_not_empty);
    do_mutex_lock_release(mbox[mbox_idx].mutex_lock);

    return cond[mbox[mbox_idx].cond_not_full].block_count;
}

int do_mbox_recv(int mbox_idx, void * msg, int msg_length)
{
    mlocks[mbox[mbox_idx].mutex_lock].lock.status = UNLOCKED;

    do_mutex_lock_acquire(mbox[mbox_idx].mutex_lock);
    while(mbox[mbox_idx].buf_used - msg_length < 0){
        do_condition_wait(mbox[mbox_idx].cond_not_empty, mbox[mbox_idx].mutex_lock);
        cond[mbox[mbox_idx].cond_not_empty].block_count++;
    }
    mbox[mbox_idx].buf_used -= msg_length;

    uint8_t* src = mbox[mbox_idx].msg;
    uint8_t* dst = msg;

    for (int i = 0; i < msg_length; i++){
        dst[i] = src[(mbox[mbox_idx].start + i) % MAX_MBOX_LENGTH];
    }
    mbox[mbox_idx].start = (mbox[mbox_idx].start + msg_length) % MAX_MBOX_LENGTH;

    do_condition_broadcast(mbox[mbox_idx].cond_not_full);
    do_mutex_lock_release(mbox[mbox_idx].mutex_lock);

    return cond[mbox[mbox_idx].cond_not_empty].block_count;
}