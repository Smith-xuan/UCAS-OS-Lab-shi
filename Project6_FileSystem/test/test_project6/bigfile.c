#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char buff[64];

int main(void)
{
    int fd = sys_fopen("1.txt", 3);
    // write 'hello world!' * 10
    for (int i = 0; i < 9; i++)
    {
        sys_fwrite(fd, "hello world!\n", 13);
        sys_lseek(fd, 1<<20, 1);
    }

    // read
    sys_lseek(fd,0,0);
    for (int i = 0; i < 9; i++)
    {
        sys_fread(fd, buff, 13);
        for (int j = 0; j < 13; j++)
        {
            printf("%c", buff[j]);
        }
        sys_lseek(fd, 1<<20, 1);
    }

    sys_fclose(fd);

    return 0;
}