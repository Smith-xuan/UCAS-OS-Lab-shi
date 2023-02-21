#ifndef __INCLUDE_LOADER_H__
#define __INCLUDE_LOADER_H__

#include <type.h>
#define SECTOR_SIZE 512
#define USER_TEXT_BASE 0x10000
uint64_t load_task_img(int task_id, uintptr_t pgdir);

#endif