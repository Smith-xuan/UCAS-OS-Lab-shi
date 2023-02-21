#ifndef __INCLUDE_OS_FS_H__
#define __INCLUDE_OS_FS_H__

#include <type.h>

/* macros of file system */
#define SUPERBLOCK_MAGIC 0x20221205
#define NUM_FDESCS 16

#define BS       (1 << 12) /*Block  size  4KB*/
#define SS       (1 << 9 ) /*Sector size 512B*/
#define FS_SIZE  (1 << 20) /*FS have 1M  SS*/
#define FS_START (1 << 20) /*FS start block num*/
#define ISZIE    128
#define DSIZE    32

#define SB_SD_OFFSET     0  /*1 sector*/
#define BMAP_SD_OFFST    1  /*32 sector(512MB)*/
#define IMAP_SD_OFFSET  33  /*1 sector(512*512B)*/
#define INODE_SD_OFFSET 34  /*512 sector*/
#define DATA_SD_OFFSET  546 /*almost 512MB*/


#define SB_MEM_ADDR       0x5d000000
#define BMAP_MEM_ADDR     0x5d000200
#define IMAP_MEM_ADDR     0x5d004200
#define INODE_MEM_ADDR    0x5d004400
#define LEV1_MEM_ADDR     0x5d044400
#define DATA_MEM_ADDR     0x5d048400


#define DPTR 10
#define MAX_FILE_NAME 24
#define DIR  0
#define FILE 1
#define R_A  1
#define W_A  2
#define RW_A 3

/* data structures of file system */
typedef struct superblock_t{
    // TODO [P6-task1]: Implement the data structure of superblock
    uint32_t magic;//magic number,to check if the fs exits

    uint32_t fs_size;//the size of file system
    uint32_t fs_start;//the start location of the file system

    uint32_t blockmap_num;//the sector num which the blockmap occupy
    uint32_t blockmap_start;//the start location of the blockmap

    uint32_t inodemap_num;
    uint32_t inodemap_start;//as above

    uint32_t datablock_num;
    uint32_t datablock_start;//one bit of bmap for one block

    uint32_t inode_num;
    uint32_t inode_start;//ont bit of inodemap for one sector

    uint32_t isize;
    uint32_t dsize;

} superblock_t;

typedef struct dentry_t{
    // TODO [P6-task1]: Implement the data structure of directory entry
    uint32_t mode; //file or dir
    uint32_t ino; //the number of inode
    char name[MAX_FILE_NAME];
} dentry_t;

typedef struct inode_t{ 
    // TODO [P6-task1]: Implement the data structure of inode
    uint8_t mode;
    uint8_t access; //R W EXE
    uint16_t nlinks; //link num
    uint32_t ino;  //the number of inode
    uint32_t size; //the file or dir size
    uint32_t direct_ptr[DPTR]; //direct dir
    uint32_t lev1_ptr[3];
    uint32_t lev2_ptr[2];
    uint32_t lev3_ptr;
    uint32_t used_size;
    uint64_t mtime; // modified time
    uint64_t stime; // start time
    uint64_t aligned[4];
} inode_t;

typedef struct fdesc_t{
    // TODO [P6-task2]: Implement the data structure of file descriptor
    uint32_t ino;
    uint32_t r_ptr;
    uint32_t w_ptr;
    uint8_t access;
} fdesc_t;

/* modes of do_fopen */
#define O_RDONLY 1  /* read only open */
#define O_WRONLY 2  /* write only open */
#define O_RDWR   3  /* read/write open */

/* whence of do_lseek */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* fs function declarations */
extern int do_mkfs(void);
extern int do_statfs(void);
extern int do_cd(char *path);
extern int do_mkdir(char *path);
extern int do_rmdir(char *path);
extern int do_ls(char *path, int option);
extern int do_touch(char *path);
extern int do_cat(char *path);
extern int do_fopen(char *path, int mode);
extern int do_fread(int fd, char *buff, int length);
extern int do_fwrite(int fd, char *buff, int length);
extern int do_fclose(int fd);
extern int do_ln(char *src_path, char *dst_path);
extern int do_rm(char *path);
extern int do_lseek(int fd, int offset, int whence);

#endif