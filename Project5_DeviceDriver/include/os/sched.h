/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *        Process scheduling related content, such as: scheduler, process blocking,
 *                 process wakeup, process creation, process kill, etc.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#ifndef INCLUDE_SCHEDULER_H_
#define INCLUDE_SCHEDULER_H_

#include <type.h>
#include <os/list.h>
#include <os/lock.h>

#define NUM_MAX_TASK 32




/* used to save register infomation */
typedef struct regs_context
{
    /* Saved main processor registers.*/
    reg_t regs[32];

    /* Saved special registers. */
    reg_t sstatus;
    reg_t sepc;
    reg_t sbadaddr;
    reg_t scause;
} regs_context_t;

/* used to save register infomation in switch_to */
typedef struct switchto_context
{
    /* Callee saved registers.*/
    reg_t regs[14];
} switchto_context_t;

typedef enum {
    TASK_BLOCKED,
    TASK_RUNNING,
    TASK_READY,
    TASK_EXITED,
} task_status_t;

/* Process Control Block */
typedef struct pcb
{
    /* register context */
    // NOTE: this order must be preserved, which is defined in regs.h!!
//    switchto_context_t sw_context;
//    regs_context_t reg_context;
    reg_t kernel_sp;
    reg_t user_sp;
    ptr_t kernel_stack_base;
    ptr_t user_stack_base;

    /* previous, next pointer */
    list_node_t list;
    list_head wait_list;
    list_head plist;
    int pnum;
    /* entry point */
    reg_t entry_point;

    /* lock num */
    int lock_num;
    mutex_lock_t *lock[LOCK_NUM];

    /* pgdir */
    uintptr_t pgdir;

    /* process id */
    pid_t pid;

    /* thread id */
    pid_t tid;

    /* thread num */
    int thread_num;

    /* thread idx */
    int thread_idx[5];

    /* BLOCK | READY | RUNNING */
    task_status_t status;

    /* share memory page num */
    int shm_pgnum;

    /* current core id */
    uint64_t cpu_id;

    /* taskset type */
    int type;

    /* cursor position */
    int cursor_x;
    int cursor_y;

    char* name;

    /* time(seconds) to wake up sleeping PCB */
    uint64_t wakeup_time;

} pcb_t;

/* ready queue to run */
extern list_head ready_queue;

/* sleep queue to be blocked in */
extern list_head sleep_queue;

/* current running task PCB */
extern pcb_t * volatile current_running[2];
extern pcb_t * volatile next_running[2];
extern pid_t process_id;

extern pcb_t pcb[NUM_MAX_TASK];
extern pcb_t pid0_pcb;
extern pcb_t pid0_pcb_slave;
extern const ptr_t pid0_stack;
extern const ptr_t pid0_stack_slave;

extern void ret_from_exception();

extern void switch_to(pcb_t *prev, pcb_t *next);

void do_scheduler(void);
void do_sleep(uint32_t);

void do_block(list_node_t *, list_head *queue);
void do_unblock(list_node_t *);
void thread_create(pid_t *thread, void(*thread_func)(void *), void *argv);
void init_pcb_stack(ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,pcb_t *pcb);

/* TODO [P3-TASK1] exec exit kill waitpid ps*/
#ifdef S_CORE
extern pid_t do_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2);
#else
extern pid_t do_exec(char *name, int argc, char *argv[]);
#endif
extern void do_exit(void);
extern int do_kill(pid_t pid, pid_t tid);
extern int do_waitpid(pid_t pid);
extern void do_process_show();
extern pid_t do_taskset(char *name, int argc, char *argv[], int type);
extern void do_tasksetrun(pid_t pid, int type);
extern pid_t do_getpid();

typedef pid_t pthread_t;
void pthread_create(pthread_t *thread, void (*start_routine)(void*), void *arg);


#endif
