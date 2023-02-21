#include <syscall.h>
#include <stdint.h>
#include <unistd.h>

static const long IGNORE = 0L;

static long invoke_syscall(long sysno, long arg0, long arg1, long arg2,
                           long arg3, long arg4)
{
    /* TODO: [p2-task3] implement invoke_syscall via inline assembly */
    long result = 0;
    asm volatile("mv a7, %[sysno]\n"
                 "mv a0, %[arg0]\n"
                 "mv a1, %[arg1]\n"
                 "mv a2, %[arg2]\n"
                 "mv a3, %[arg3]\n"
                 "mv a4, %[arg4]\n"
                 "ecall\n"
                 "mv %[result], a0"
                :[result]"=r"(result)
                :[sysno]"r"(sysno),
                  [arg0]"r" (arg0),
                  [arg1]"r" (arg1),
                  [arg2]"r" (arg2),
                  [arg3]"r" (arg3),
                  [arg4]"r" (arg4)
                );

    return result;
}

void sys_yield(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_yield */
    invoke_syscall(SYSCALL_YIELD, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_move_cursor(int x, int y)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_move_cursor */
    invoke_syscall(SYSCALL_CURSOR, x, y, IGNORE, IGNORE, IGNORE);
}

void sys_write(char *buff)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_write */
    invoke_syscall(SYSCALL_WRITE, buff, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_reflush(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_reflush */
    invoke_syscall(SYSCALL_REFLUSH, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_mutex_init(int key)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_init */
    int mlock_idx = invoke_syscall(SYSCALL_LOCK_INIT, key, IGNORE, IGNORE, IGNORE, IGNORE);
    return mlock_idx;
}

void sys_mutex_acquire(int mutex_idx)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_acquire */
    invoke_syscall(SYSCALL_LOCK_ACQ, mutex_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_mutex_release(int mutex_idx)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_release */
    invoke_syscall(SYSCALL_LOCK_RELEASE, mutex_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

long sys_get_timebase(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_get_timebase */
    long timebase = invoke_syscall(SYSCALL_GET_TIMEBASE, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
    return timebase;
}

long sys_get_tick(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_get_tick */
    long tick = invoke_syscall(SYSCALL_GET_TICK, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
    return tick;
}

void sys_sleep(uint32_t time)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_sleep */
    invoke_syscall(SYSCALL_SLEEP, time, IGNORE, IGNORE, IGNORE, IGNORE);
}

void thread_create(int32_t *thread, void(*thread_func)(void *), void *argv)
{
    invoke_syscall(SYSCALL_THREAD_CREATE, thread, thread_func, argv, IGNORE, IGNORE);
}

// S-core
// pid_t  sys_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2)
// {
//     /* TODO: [p3-task1] call invoke_syscall to implement sys_exec for S_CORE */
// }    
// A/C-core
pid_t  sys_exec(char *name, int argc, char **argv)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_exec */
    pid_t pid = invoke_syscall(SYSCALL_EXEC, name, argc, argv, IGNORE, IGNORE);
    return pid;
}

void sys_backspace(void)
{
    invoke_syscall(SYSCALL_BACKSPACE, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}


void sys_exit(void)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_exit */
    invoke_syscall(SYSCALL_EXIT, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

int  sys_kill(pid_t pid, pid_t tid)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_kill */
    int status = invoke_syscall(SYSCALL_KILL, pid, tid, IGNORE, IGNORE, IGNORE);
    return status;
}

int  sys_waitpid(pid_t pid)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_waitpid */
    int status = invoke_syscall(SYSCALL_WAITPID, pid, IGNORE, IGNORE, IGNORE, IGNORE);
    return status;
}


void sys_ps(void)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_ps */
    invoke_syscall(SYSCALL_PS, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

pid_t sys_getpid()
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_getpid */
    pid_t pid = invoke_syscall(SYSCALL_GETPID, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
    return pid;
}

int  sys_getchar(void)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_getchar */
    int c = invoke_syscall(SYSCALL_GET_CHAR, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
    return c;
}

void sys_putchar(void)
{
    invoke_syscall(SYSCALL_PUT_CHAR, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_clear(void){
    invoke_syscall(SYSCALL_CLEAR, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

int  sys_barrier_init(int key, int goal)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrier_init */
    int barr_idx = invoke_syscall(SYSCALL_BARR_INIT, key, goal, IGNORE, IGNORE, IGNORE);
    return barr_idx;
}

void sys_barrier_wait(int bar_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrie_wait */
    invoke_syscall(SYSCALL_BARR_WAIT, bar_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_barrier_destroy(int bar_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrie_destory */
    invoke_syscall(SYSCALL_BARR_DESTROY, bar_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_condition_init(int key)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_init */
    int cond_idx = invoke_syscall(SYSCALL_COND_INIT, key, IGNORE, IGNORE, IGNORE, IGNORE);
    return cond_idx;
}

void sys_condition_wait(int cond_idx, int mutex_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_wait */
    invoke_syscall(SYSCALL_COND_WAIT, cond_idx, mutex_idx, IGNORE, IGNORE, IGNORE);
}

void sys_condition_signal(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_signal */
    invoke_syscall(SYSCALL_COND_SIGNAL, cond_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_condition_broadcast(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_broadcast */
    invoke_syscall(SYSCALL_COND_BROADCAST, cond_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_condition_destroy(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_destroy */
    invoke_syscall(SYSCALL_COND_DESTROY, cond_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_mbox_open(char * name)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_open */
    int mbox_idx = invoke_syscall(SYSCALL_MBOX_OPEN, name, IGNORE, IGNORE, IGNORE, IGNORE);
    return mbox_idx;
}

void sys_mbox_close(int mbox_id)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_close */
    invoke_syscall(SYSCALL_MBOX_CLOSE, mbox_id, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_mbox_send(int mbox_idx, void *msg, int msg_length)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_send */
    int block_count = invoke_syscall(SYSCALL_MBOX_SEND, mbox_idx, msg, msg_length, IGNORE, IGNORE);
    return block_count;
}

int sys_mbox_recv(int mbox_idx, void *msg, int msg_length)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_recv */
    int block_count = invoke_syscall(SYSCALL_MBOX_RECV, mbox_idx, msg, msg_length, IGNORE, IGNORE);
    return block_count;
}

pid_t sys_taskset(char *name, int argc, char **argv, int type)
{
    pid_t pid = invoke_syscall(SYSCALL_TASKSET, name, argc, argv, type, IGNORE);
    return pid;
}

void sys_tasksetrun(pid_t pid, int type)
{
    invoke_syscall(SYSCALL_TASKSET_RUN, pid, type, IGNORE, IGNORE,IGNORE);
}

void pthread_create_t(pthread_t *thread,
                   void (*start_routine)(void*),
                   void *arg)
{
    /* TODO: [p4-task4] implement pthread_create */
    invoke_syscall(SYSCALL_PTHREAD_CREATE, thread, start_routine, arg, IGNORE,IGNORE);
}

void* sys_shmpageget(int key)
{
    /* TODO: [p4-task5] call invoke_syscall to implement sys_shmpageget */
    uintptr_t addr = invoke_syscall(SYSCALL_SHM_GET, key, IGNORE, IGNORE, IGNORE,IGNORE);
    return addr;
}

void sys_shmpagedt(void *addr)
{
    /* TODO: [p4-task5] call invoke_syscall to implement sys_shmpagedt */
    invoke_syscall(SYSCALL_SHM_DT, addr, IGNORE, IGNORE, IGNORE,IGNORE);
}

int sys_net_send(void *txpacket, int length)
{
    /* TODO: [p5-task1] call invoke_syscall to implement sys_net_send */
    int rt_length = invoke_syscall(SYSCALL_NET_SEND, txpacket, length,IGNORE,IGNORE,IGNORE);
    return rt_length;
}

int sys_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
    /* TODO: [p5-task2] call invoke_syscall to implement sys_net_recv */
    int all_length = invoke_syscall(SYSCALL_NET_RECV, rxbuffer, pkt_num, pkt_lens,IGNORE,IGNORE);
    return all_length;
}