/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                       System call related processing
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

#ifndef INCLUDE_SYSCALL_H_
#define INCLUDE_SYSCALL_H_

#include <os/sched.h>
#include <type.h>

#define NUM_SYSCALLS 96

/* syscall id */
#define SYSCALL_EXEC 0
#define SYSCALL_EXIT 1
#define SYSCALL_SLEEP 2
#define SYSCALL_KILL 3
#define SYSCALL_WAITPID 4
#define SYSCALL_PS 5
#define SYSCALL_GETPID 6
#define SYSCALL_YIELD 7
#define SYSCALL_THREAD_CREATE 8
#define SYSCALL_PUT_CHAR 9
#define SYSCALL_WRITE 20
#define SYSCALL_READCH 21
#define SYSCALL_CURSOR 22
#define SYSCALL_REFLUSH 23
#define SYSCALL_CLEAR 24
#define SYSCALL_GET_CHAR 25
#define SYSCALL_GET_TIMEBASE 30
#define SYSCALL_GET_TICK 31
#define SYSCALL_LOCK_INIT 40
#define SYSCALL_LOCK_ACQ 41
#define SYSCALL_LOCK_RELEASE 42
#define SYSCALL_SHOW_TASK 43
#define SYSCALL_BARR_INIT 44
#define SYSCALL_BARR_WAIT 45
#define SYSCALL_BARR_DESTROY 46
#define SYSCALL_COND_INIT 47
#define SYSCALL_COND_WAIT 48
#define SYSCALL_COND_SIGNAL 49
#define SYSCALL_COND_BROADCAST 50
#define SYSCALL_COND_DESTROY 51
#define SYSCALL_MBOX_OPEN 52
#define SYSCALL_MBOX_CLOSE 53
#define SYSCALL_MBOX_SEND 54
#define SYSCALL_MBOX_RECV 55
#define SYSCALL_SHM_GET 56
#define SYSCALL_SHM_DT 57

#define SYSCALL_NET_SEND 63
#define SYSCALL_NET_RECV 64

#define SYSCALL_FS_MKFS 65
#define SYSCALL_FS_STATFS 66
#define SYSCALL_FS_CD 67
#define SYSCALL_FS_MKDIR 68
#define SYSCALL_FS_RMDIR 69
#define SYSCALL_FS_LS 70
#define SYSCALL_FS_TOUCH 71
#define SYSCALL_FS_CAT 72
#define SYSCALL_FS_FOPEN 73
#define SYSCALL_FS_FREAD 74 
#define SYSCALL_FS_FWRITE 75 
#define SYSCALL_FS_FCLOSE 76
#define SYSCALL_FS_LN 77
#define SYSCALL_FS_RM 78
#define SYSCALL_FS_LSEEK 90

#define SYSCALL_BACKSPACE 79
#define SYSCALL_TASKSET 80
#define SYSCALL_TASKSET_RUN 81
#define SYSCALL_PTHREAD_CREATE 82


/* syscall function pointer */
extern long (*syscall[NUM_SYSCALLS])();
extern void handle_syscall(regs_context_t *regs, uint64_t stval, uint64_t scause);



#endif
