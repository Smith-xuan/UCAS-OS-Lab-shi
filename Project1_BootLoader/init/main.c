#include <common.h>
#include <asm.h>
#include <os/bios.h>
#include <os/task.h>
#include <os/string.h>
#include <os/loader.h>
#include <type.h>

#define VERSION_BUF 50
#define TASKS_INFO_BLOCK_ID_ADDR 0x502001fa
#define TASKS_INFO_lOC 0x50300000
#define TASK_NAME_MAXNUM 8

int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];

// Task info array
task_info_t tasks[TASK_MAXNUM];

static int bss_check(void)
{
    for (int i = 0; i < VERSION_BUF; ++i)
    {
        if (buf[i] != 0)
        {
            return 0;
        }
    }
    return 1;
}

static void init_bios(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())BIOS_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
}

static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
    short tasksInfoBlockId = *(short *)TASKS_INFO_BLOCK_ID_ADDR;
    bios_sdread(TASKS_INFO_lOC,1,tasksInfoBlockId);

    task_info_t *tasksInfoPtr = (task_info_t *)TASKS_INFO_lOC;
    for (int i = 0; i < TASK_MAXNUM; i++){
        for (int j = 0; j < TASK_NAME_MAXNUM; j++){
            tasks[i].task_name[j] = tasksInfoPtr[i].task_name[j];
        }
        tasks[i].task_offset = tasksInfoPtr[i].task_offset;
        tasks[i].task_size = tasksInfoPtr[i].task_size;
        tasks[i].TaskEntryPoint = tasksInfoPtr[i].TaskEntryPoint;
    }
    
}

int main(void)
{
    // Check whether .bss section is set to zero
    int check = bss_check();

    // Init jump table provided by bBIOS (ΦωΦ)
    init_bios();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Output 'Hello OS!', bss check result and OS version
    char output_str[] = "bss check: _ version: _\n\r";
    char output_val[2] = {0};
    int i, output_val_pos = 0;

    output_val[0] = check ? 't' : 'f';
    output_val[1] = version + '0';
    for (i = 0; i < sizeof(output_str); ++i)
    {
        buf[i] = output_str[i];
        if (buf[i] == '_')
        {
            buf[i] = output_val[output_val_pos++];
        }
    }

    bios_putstr("Hello OS!\n\r");
    bios_putstr(buf);

 

/*    while (1)
    {
        c = bios_getchar();
        if (c == -1){
            continue;
        }
        else if (c == '0' | c == '1' | c == '2' | c == '3'){
            int task_id = c - 48;
            uint64_t ENTRY_POINT = load_task_img(task_id);
            asm volatile("jalr %[entry_point]"
                        ::[entry_point]"r"(ENTRY_POINT)
                        );
        }
        else if (c == 13){
            bios_putstr("\n\r");
        }
        else if (c == 127){
            bios_putstr("\b");
        }
        else
            bios_putchar(c);
    } 
*/
    int c = 0;
    char buf[TASK_NAME_MAXNUM];
    int buf_num = 0;
    while (1){
        int count = 0;
        c = bios_getchar();
        if (c == -1){
            continue;
        }
        else if (c == '\r'){
            bios_putchar('\n');
            for (int j = 0; j < TASK_MAXNUM; j++){
                if(strcmp(tasks[j].task_name,buf) == 0 && buf[0] != '\0'){
                    uint64_t ENTRY_POINT = load_task_img(j);
                    asm volatile("jalr %[entry_point]"
                            ::[entry_point]"r"(ENTRY_POINT)
                            );
                    break;              
                }
                else 
                    count++;
                
                if(count == TASK_MAXNUM){
                    bios_putchar('W');
                    bios_putchar('R');
                    bios_putchar('O');
                    bios_putchar('N');
                    bios_putchar('G');
                    bios_putchar(' ');
                    bios_putchar('N');
                    bios_putchar('A');
                    bios_putchar('M');
                    bios_putchar('E');
                    bios_putchar('\n');
                    count = 0;
                }
                    
            }
            for (int j = 0; j < TASK_NAME_MAXNUM; j++){
                buf[j] = 0;
            }
            buf_num = 0;
        }
        else if (c == 127){
            bios_putchar('\b');
            buf[--buf_num] = '\0';
        }
        else if (c == ' '){
            bios_putchar(' ');
        }
        else {
            bios_putchar(c);
            buf[buf_num++] = c;
        }
    }
    
    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.


    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        asm volatile("wfi");
    }

    return 0;
}
