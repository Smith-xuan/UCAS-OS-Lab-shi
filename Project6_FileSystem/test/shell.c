/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                  The shell acts as a task running in user mode.
 *       The main function is to make system calls through the user's output.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
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

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define SHELL_BEGIN 20

void inputParse(char *buf);

int main(void)
{
    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------- COMMAND -------------------\n");
    printf("> root@UCAS_OS: ");

    char buf[100];
    int i = 0;

    while (1)
    {
        // TODO [P3-task1]: call syscall to read UART port
        char c;
        int n;
        while((n = sys_getchar()) == -1) ;//if no input, wait
        
        // TODO [P3-task1]: parse input
        // note: backspace maybe 8('\b') or 127(delete)
        c = (char)n;
        if(c == 8 || c == 127){
            if(i > 0){
//              printf("\b");
//              i--;
                sys_backspace();
                i--;
            }
        }
        else if(c == '\r'){
            printf("\n");
            buf[i] = '\0';
            printf("%c", c);
            inputParse(buf);
            i = 0;
            printf("> root@UCAS_OS: ");
        }else{
            buf[i++] = c;
            printf("%c",c);
        }
        // TODO [P3-task1]: ps, exec, kill, clear        

        // TODO [P6-task1]: mkfs, statfs, cd, mkdir, rmdir, ls

        // TODO [P6-task2]: touch, cat, ln, ls -l, rm
    }

    return 0;
}

int atoi(char *buf){
    int res = 0;
    int i = 0;
    while(buf[i] != '\0'){
       res = res*10 + buf[i] - '0';
       i++;
    }
    return res;
}

void inputParse(char *buf)
{
    char arg[16][16];
    int pid_num = 0;
    int i = 0;
    int j = 0;

    //get the commands
    while(buf[j] != '\0' && buf[j] != ' '){
        arg[0][i] = buf[j];
        i++;
        j++;
    }
    arg[0][i] = '\0';

    //get the pid(or file name)
    if(buf[j] == ' '){
        j++;
        i = 0;
        while(buf[j] != '\0' && buf[j] != ' '){
            arg[1][i] = buf[j];
            i++;
            j++;
        }
        arg[1][i] = '\0';
        pid_num = atoi(arg[1]);
    }

    if(!strcmp(arg[0], "clear")){
        sys_clear();
        sys_move_cursor(0,SHELL_BEGIN);
        printf("------------------- COMMAND -------------------\n");
    }else if(!strcmp(arg[0], "ps")){
        sys_ps();
    }else if(!strcmp(arg[0], "exit")){
        sys_exit();
    }else if(!strcmp(arg[0], "waitpid")){
        int status;
        status = sys_waitpid(pid_num);
        if(status)
            printf("waitpid successfully, pid = %d...\n", pid_num);
        else
            printf("Error: can not find the running process\n");
    }else if(!strcmp(arg[0], "kill")){
        int status_k;
        int tid_num;
        if(buf[j] == ' '){
            j++;
            i = 0;
            while(buf[j] != '\0' && buf[j] != ' '){
                arg[2][i] = buf[j];
                i++;
                j++;
            }
            arg[2][i] = '\0';
            tid_num = atoi(arg[2]);
        }        
        status_k = sys_kill(pid_num, tid_num);
        if(status_k)
            printf("kill successfully, pid = %d tid = %d...\n",pid_num,tid_num);
        else
            printf("Error: can not find the running process\n");
    }else if(!strcmp(arg[0], "taskset")){
        int pid_taskset;
        char *argv[1]; //now think of filename only
        int argc;
        if(!strcmp(arg[1], "0x1")){
            if(buf[j] == ' '){
                j++;
                i = 0;
                while(buf[j] != '\0' && buf[j] != ' '){
                    arg[2][i] = buf[j];
                    i++;
                    j++;
                }
                arg[2][i] = '\0';
            }
            char *filename = arg[2];
            argv[0] = arg[2];
            argc = 1;
            int type = 1;
            int pid_taskset = (int)sys_taskset(filename, argc, argv, type);
            printf("taskset succssfully, pid = %d\n", pid_taskset);
        }else if(!strcmp(arg[1], "0x2")){
            if(buf[j] == ' '){
                j++;
                i = 0;
                while(buf[j] != '\0' && buf[j] != ' '){
                    arg[2][i] = buf[j];
                    i++;
                    j++;
                }
                arg[2][i] = '\0';
            }
            char *filename = arg[2];
            argv[0] = arg[2];
            argc = 1;
            int type = 2;
            int pid_taskset = (int)sys_taskset(filename, argc, argv, type);  
            printf("taskset succssfully, pid = %d\n", pid_taskset);          
        }else if(!strcmp(arg[1], "0x3")){
            if(buf[j] == ' '){
                j++;
                i = 0;
                while(buf[j] != '\0' && buf[j] != ' '){
                    arg[2][i] = buf[j];
                    i++;
                    j++;
                }
                arg[2][i] = '\0';
            }
            char *filename = arg[2];
            argv[0] = arg[2];
            argc = 1;
            int type = 0;
            int pid_taskset = (int)sys_taskset(filename, argc, argv, type);
            printf("taskset succssfully, pid = %d\n", pid_taskset);            
        }
        else if(!strcmp(arg[1], "-p")){
            if(buf[j] == ' '){
                j++;
                i = 0;
                while(buf[j] != '\0' && buf[j] != ' '){
                    arg[2][i] = buf[j];
                    i++;
                    j++;
                }
                arg[2][i] = '\0';
            }

            if(!strcmp(arg[2], "0x1")){
                if(buf[j] == ' '){
                    j++;
                    i = 0;
                    while(buf[j] != '\0' && buf[j] != ' '){
                        arg[3][i] = buf[j];
                        i++;
                        j++;
                    }
                    arg[3][i] = '\0';
                }
                int pid_taskset = atoi(arg[3]);
                int type = 1;
                sys_tasksetrun(pid_taskset, type);
            }else if(!strcmp(arg[2], "0x2")){
                if(buf[j] == ' '){
                    j++;
                    i = 0;
                    while(buf[j] != '\0' && buf[j] != ' '){
                        arg[3][i] = buf[j];
                        i++;
                        j++;
                    }
                    arg[3][i] = '\0';
                }
                int pid_taskset = atoi(arg[3]);
                int type = 2;
                sys_tasksetrun(pid_taskset, type);
            }else if(!strcmp(arg[2], "0x3")){
                if(buf[j] == ' '){
                    j++;
                    i = 0;
                    while(buf[j] != '\0' && buf[j] != ' '){
                        arg[3][i] = buf[j];
                        i++;
                        j++;
                    }
                    arg[3][i] = '\0';
                }
                int pid_taskset = atoi(arg[3]);
                int type = 0;
                sys_tasksetrun(pid_taskset, type);
            }else{
                printf("Unknown Command!\n");
            }
        }else{
            printf("Unknown Command!\n");
        }

    }else if(!strcmp(arg[0], "exec")){
        int pid_exec;
        int argc = 0;
        char *argv[4];
        int m,n;
        int num = 0;
        int wait_flag = 0;

        //get the argv(at most 3, indeed m-2)
        for (m = 2; m < 5; m++){
            if(buf[j] == '\0'){
                break;
            }else if(buf[j] == ' '){
                num = 0;
                j++;
                while(buf[j] != '\0' && buf[j] !=' '){
                    arg[m][num] = buf[j];
                    num++;
                    j++;
                }
            }
            arg[m][num] = '\0';
        }

        char *filename = arg[1];

        if(arg[m-1][0] == '&'){
            argc = m-2;
            wait_flag = 0;
        }else{
            argc = m-1;
            wait_flag = 1;
        }
        
        for (n = 0; n < argc+1; n++){
            argv[n] = arg[1+n];
        }
        
        pid_exec = (int)sys_exec(filename, argc, argv);
        if(wait_flag){
            sys_waitpid(pid_exec);
        }
        if(pid_exec != 0)
            printf("execute %s successfully, pid = %d...\n",filename,pid_exec);
        else
            printf("can not find files\n");
        
    }else if(!strcmp(arg[0], "ls")){
        if(!strcmp(arg[1], "-l")){
            sys_ls(arg[1], 1);
        }else{
            sys_ls(arg[1], 0);
        }
    }else if(!strcmp(arg[0], "cd")){
        sys_cd(arg[1]);
    }else if(!strcmp(arg[0], "mkfs")){
        sys_mkfs();
    }else if(!strcmp(arg[0], "statfs")){
        sys_statfs();
    }else if(!strcmp(arg[0], "mkdir")){
        sys_mkdir(arg[1]);
    }else if(!strcmp(arg[0], "rmdir")){
        sys_rmdir(arg[1]);
    }else if(!strcmp(arg[0], "touch")){
        sys_touch(arg[1]);
    }else if(!strcmp(arg[0], "cat")){
        sys_cat(arg[1]);
    }else if(!strcmp(arg[0], "ln")){
        if(buf[j++] == ' '){
            i = 0;
            while(buf[j] != '\0' && buf[j] != ' '){
                arg[2][i++] = buf[j++];
            }
            arg[2][i] = '\0';
        }
        sys_ln(arg[1],arg[2]);
    }else if(!strcmp(arg[0], "rm")){
        sys_rm(arg[1]);
    }
    else{
        printf("Unknown Command!\n");
    }

}