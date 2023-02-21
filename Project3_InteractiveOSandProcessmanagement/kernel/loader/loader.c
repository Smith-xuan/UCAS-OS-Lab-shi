#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>

uint64_t load_task_img(int taskid)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */
    unsigned int block_num = tasks[taskid].task_size;
    unsigned int block_entry = tasks[taskid].task_offset;
//    uint64_t ENTRY_POINT = TASK_MEM_BASE+taskid*TASK_SIZE;
    uint64_t ENTRY_POINT = tasks[taskid].TaskEntryPoint;
    bios_sdread(ENTRY_POINT,block_num,block_entry);


    return ENTRY_POINT;
}