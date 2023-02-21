#include <stdio.h>
#include <unistd.h>  // NOTE: use this header after implementing syscall!
#include <kernel.h>

#define ARRNUM 10000
int num[ARRNUM];
int flag;

void thread_1(void *th)
{
    int *sel = (int *)th;
    if (*sel == 1){
        for (int i = 0; i < ARRNUM/2; i++){
            num[i] = i;
            if(i%1000 == 0){
                sys_move_cursor(0,6);
                printf("> [TASK] This is in the 1st thread! (%d)\n", i);
                sys_sleep(1);
            }
        }
    }
    else if(*sel == 2){
        for (int i = ARRNUM/2; i < ARRNUM; i++){
            num[i] = i;
            if(i%1000 == 0){
                sys_move_cursor(0,7);
                printf("> [TASK] This is in the 2nd thread! (%d)\n", i);
                sys_sleep(1);
            }
        }
    }
    
    flag = 1;
    while(1) ;
}

// void thread_2(void *th2)
// {
//     for (int i = ARRNUM/2; i < ARRNUM; i++){
//         num[i] = i;
//         if(i%1000 == 0){
//             sys_move_cursor(0,7);
//             printf("> [TASK] This is in the 2nd thread! (%d)\n", i);
//             sys_sleep(1);
//         }
//     }
//     flag[1] = 1;
//     while(1) ;
// }

int main()
{
    int sum = 0;
    flag = 0;

    int32_t pthread_1 = 1, pthread_2 = 2;
    int th1 = 1,th2 = 2;
    thread_create(&pthread_1, thread_1, &th1);
    thread_create(&pthread_2, thread_1, &th2);

    while(!flag) ;

    for (int i = 0; i < ARRNUM; i++){
        sum += num[i];
    }
    

    sys_move_cursor(0,8);
    printf("> [TASK] Two threads finished, the sum = %d! \n", sum);
    
    while(1) ;
}