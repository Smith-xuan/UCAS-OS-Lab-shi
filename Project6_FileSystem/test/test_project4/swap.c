#include <stdio.h>
#include <syscall.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#define MAGIC 250


int main()
{
    sys_move_cursor(1, 1);
    printf("Begin page swap testing. Please note swap infomation.\n");
    long* pf1 = 0x10000000;
    long* pf2 = 0x20000000;
    long* pf3 = 0x30000000;
    long* pf4 = 0x40000000;
    long* pf5 = 0x50000000;
    long* pf6 = 0x60000000;
    *pf1 = MAGIC;
    sys_move_cursor(1, 2);
    printf("Modify pf1\n");
    *pf2 = MAGIC;
    sys_move_cursor(1, 3);
    printf("Modify pf2\n");
    *pf3 = MAGIC;
    sys_move_cursor(1, 4);
    printf("Modify pf3\n");
    *pf4 = MAGIC;
    sys_move_cursor(1, 5);
    printf("Modify pf4\n");
    *pf5 = MAGIC;
    sys_move_cursor(1, 6);
    printf("Modify pf5\n");
    *pf6 = MAGIC;
    sys_move_cursor(1, 7);
    printf("Modify pf6\n");
    if(*pf1 != MAGIC){
        printf("Error, pf1!\n");
        return 0;
    }
    if(*pf2 != MAGIC){
        printf("Error, pf2!\n");
        return 0;
    }
    if(*pf3 != MAGIC){
        printf("Error, pf3!\n");
        return 0;
    }
    if(*pf4 != MAGIC){
        printf("Error, pf4!\n");
        return 0;
    }
    if(*pf5 != MAGIC){
        printf("Error, pf5!\n");
        return 0;
    }
    if(*pf6 != MAGIC){
        printf("Error, pf2!\n");
        return 0;
    }
    sys_move_cursor(1,8);
    printf("Test Succeed!\n");
    printf("End testing.\n");
    return 0;
}