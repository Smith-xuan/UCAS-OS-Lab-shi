#ifndef __INCLUDE_TASK_H__
#define __INCLUDE_TASK_H__

#include <type.h>

#define TASK_MEM_BASE    0x52000000
#define TASK_MAXNUM      16
#define TASK_SIZE        0x10000
#define TASKNAME_MAXNUM  8

/* TODO: [p1-task4] implement your own task_info_t! */
typedef struct {
    char task_name[TASKNAME_MAXNUM];
    int task_offset;
    int task_size;
    uint64_t TaskEntryPoint;
} task_info_t;

extern task_info_t tasks[TASK_MAXNUM];

#endif