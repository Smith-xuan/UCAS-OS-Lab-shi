#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <os/loader.h>
#include <os/mm.h>
#include <type.h>

uint64_t load_task_img(int taskid, uintptr_t pgdir)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */
    unsigned int block_num = tasks[taskid].mem_size;
    unsigned int fileblock_num = tasks[taskid].task_size;
    unsigned int block_entry = tasks[taskid].task_offset;
//    uint64_t ENTRY_POINT = TASK_MEM_BASE+taskid*TASK_SIZE;
//    uint64_t ENTRY_POINT = tasks[taskid].TaskEntryPoint;
    int pagenum = (block_num / 8) + 1;
    int filepagenum = (fileblock_num / 8) + 1;
    for (int i = 0; i < pagenum; i++){
        if(i < filepagenum){ // load the text
            uintptr_t kva_text = alloc_page_helper(USER_TEXT_BASE+i*PAGE_SIZE, pgdir);
            int block_id = block_entry + i*8;
            bios_sdread(kva2pa(kva_text), 8, block_id);
        }else{ // bss
            uintptr_t kva_bss = alloc_page_helper(USER_TEXT_BASE+i*PAGE_SIZE, pgdir);
            for (int j = 0; j < 512; j++){
                *((uint64_t*)kva_bss + j) = 0;
            }
        }
    }
    uint64_t ENTRY_POINT = USER_TEXT_BASE;
 //   bios_sdread(ENTRY_POINT,block_num,block_entry);

    return ENTRY_POINT;
}