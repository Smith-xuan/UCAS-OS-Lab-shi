Project6

一、设计流程：
文件系统的设计流程主要集中在如下几个方面(其余的函数主要设计思想来源于以下步骤)：
1. 磁盘上各个功能区域的分布
2. 一些关键数据结构(superblock, inode, dentry, fdesc)
3. 创建文件系统的步骤
4. 创建目录的步骤
5. 创建文件的步骤
6. 文件读写的步骤


二、对设计流程中关键点的代码诠释：

1）磁盘上各个功能区域的分布

    #define SB_SD_OFFSET     0  /*1 sector*/
    #define BMAP_SD_OFFST    1  /*32 sector(512MB)*/
    #define IMAP_SD_OFFSET  33  /*1 sector(512*512B)*/
    #define INODE_SD_OFFSET 34  /*512 sector*/
    #define DATA_SD_OFFSET  546 /*almost 512MB*/

我们由此可以知道功能区域的分布，按照SUPERBLOCK, BLOCK MAP, INODE MAP, INODE, DATE BLOCK五个区域依次分布
且大小分别是SUPERBLOCK 1个扇区、BLOCK MAP 32个扇区、INODE MAP 1个扇区、INODE 512个扇区、 DATA BLOCK 接近512MB

2）一些关键数据结构(superblock, inode, dentry, fdesc)

    typedef struct superblock_t{
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
        uint32_t mode; //file or dir
        uint32_t ino; //the number of inode
        char name[MAX_FILE_NAME];
    } dentry_t;

    typedef struct inode_t{ 
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
        uint32_t ino;
        uint32_t r_ptr;
        uint32_t w_ptr;
        uint8_t access;
    } fdesc_t;

3）创建文件系统的步骤(mkfs)
我们可以总结mkfs的主要步骤
1.读出superblock块并进行初始化
2.把初始化用到的区域在block map中标记为已用(这个区域包括SUPERBLOCK, BLOCK MAP, INODE MAP和第一个INODE即根目录)
3.把根目录的inode在imap中标记出来
4.初始化根目录的iNode，并把上述在内存中写下的东西写入磁盘
5.给根目录加入.和..这两个目录项，并把当前目录设置为根目录

    int do_mkfs(void)
    {
        // TODO [P6-task1]: Implement do_mkfs
        int i;
        superblock_t *sp = (superblock_t*)(pa2kva(SB_MEM_ADDR));
        screen_reflush();

        printk("[FS] Start initializing filesystem!\n");

        //Superblock init!
        printk("[FS] Setting superblock...\n");
        clears(sp);
        //1.fill SP
        sp -> magic    = SUPERBLOCK_MAGIC;
        sp -> fs_size  = FS_SIZE;
        sp -> fs_start = FS_START;
        sp -> blockmap_num = 32;
        sp -> blockmap_start = BMAP_SD_OFFST;
        sp -> inodemap_num = 1;
        sp -> inodemap_start = IMAP_SD_OFFSET;
        sp -> inode_num = 512;
        sp -> inode_start = INODE_SD_OFFSET;
        sp -> datablock_num = 1048576 - 546;
        sp -> datablock_start = DATA_SD_OFFSET;
        sp -> isize = ISZIE;
        sp -> dsize = DSIZE;
        printk("\n");
        printk("[FS] magic: 0x%x\n", sp -> magic);
        printk("[FS] num sector: %d, start sector: %d\n",sp -> fs_size, sp -> fs_start);
        printk("[FS] block map offset: %d(%d)\n", sp -> blockmap_start, sp -> blockmap_num);
        printk("[FS] inode map offset: %d(%d)\n", sp -> inodemap_start, sp -> inodemap_num);
        printk("[FS] inode offset: %d(%d)\n", sp -> inode_start, sp -> inode_num);
        printk("[FS] data offset: %d(%d)\n",sp -> datablock_start, sp -> datablock_num);
        printk("[FS] inode entry size: %dB, dir entry size: %dB\n", sp -> isize, sp -> dsize);
        printk("IENTRY SIZE:%d\n");

        //2.set block map
        printk("[FS] Setting block map...\n");
        uint8_t *bm = (uint8_t*)(pa2kva(BMAP_MEM_ADDR));
        for(i = 0; i < sp -> blockmap_num; i++){
            clears(bm + SS*i);
        }
        //Note: (512+2+32)/8 + 1 = 69
        for(i = 0; i < 69; i++){
            bm[i/8] = bm[i/8] | (1 << (i % 8));
        }
        //3.set inode map
        printk("[FS] Setting inode map...\n");
        uint8_t *im = (uint8_t*)(pa2kva(IMAP_MEM_ADDR));
        clears(im);
        im[0] |= 1; /*dir*/
        bios_sdwrite(SB_MEM_ADDR, 34, FS_START);

        //4.Setting inode
        printk("[FS] Setting inode...\n");
        inode_t *inode = (inode_t*)(pa2kva(INODE_MEM_ADDR));
        inode[0].ino    = 0;
        inode[0].mode   = DIR;
        inode[0].access = RW_A;
        inode[0].nlinks = 0;
        inode[0].size   = 4096;
        inode[0].used_size  = 64;       //two dentry . and ..
        inode[0].stime  = get_timer();
        inode[0].mtime  = get_timer();
        inode[0].lev1_ptr[0] = 0;
        inode[0].lev1_ptr[1] = 0;
        inode[0].lev1_ptr[2] = 0;
        inode[0].lev2_ptr[0] = 0;
        inode[0].lev2_ptr[1] = 0;
        inode[0].lev3_ptr = 0;
        inode[0].direct_ptr[0] = alloc_data_block();
        for(i = 1; i < DPTR; i++){
            inode[0].direct_ptr[i] = 0;
        }
        write_inode(inode[0].ino);
        //5.Init dentry
        init_dentry(inode[0].direct_ptr[0], inode[0].ino, 0);

        printk("Initializing filesystem finished!\n");
        screen_reflush();
        current_inode = inode;
        

        return 0;  // do_mkfs succeeds
    }

4）创建目录的步骤

我们可以总结mkdir的主要步骤
1.检查要创建的目录是否在父目录中已经存在
2.在iNode map中找到一个可用的iNode并进行初始化(这个过程中也找到一个data block存目录项)
3.在新创建的目录中加上.和..这两个目录项
4.在父目录中添加这一新创建的目录项
5.更新父目录的inode中相关信息

    int do_mkdir(char *path)
    {
        // TODO [P6-task1]: Implement do_mkdir
        bios_sdread(SB_MEM_ADDR, 1, FS_START);

        superblock_t *sp = (superblock_t*)(pa2kva(SB_MEM_ADDR));
        if(sp -> magic != SUPERBLOCK_MAGIC){
            printk("> Error: Fs does not exit!\n");
            return;
        }

        bios_sdread(INODE_MEM_ADDR, 1, FS_START + INODE_SD_OFFSET);
        if(state == 0){
            current_inode = (inode_t*)(pa2kva(INODE_MEM_ADDR));
            state = 1;
        }

        //1. check if this dir exit
        inode_t *father_dir = current_inode;      //Father
        inode_t *find_dir   = lookup(father_dir, path, DIR);
        if(find_dir != NULL){
            printk("This DIR has already exited\n");
            return;
        }
        //2. alloc an inode and write inode map bit 1
        uint32_t inode_snum = alloc_inode();
        if(inode_snum == 0){
            printk(">ERROR: No Available Inode!\n");
            return;
        }
        inode_t *new_inode = (inode_t*)(pa2kva(INODE_MEM_ADDR) + inode_snum*512);
        new_inode -> ino    = inode_snum;
        new_inode -> mode   = DIR;
        new_inode -> access = RW_A;
        new_inode -> nlinks = 0;
        new_inode -> size   = 4096;
        new_inode -> used_size  = 64; //. & .. dir
        new_inode -> stime = new_inode -> mtime = get_timer();
        new_inode -> direct_ptr[0] = alloc_data_block();  
        for(int i = 1; i < DPTR; i++){
            new_inode -> direct_ptr[i] = 0;
        }
        (new_inode -> lev1_ptr)[0] = 0;
        (new_inode -> lev1_ptr)[1] = 0;
        (new_inode -> lev1_ptr)[2] = 0;
        (new_inode -> lev2_ptr)[0] = 0;
        (new_inode -> lev2_ptr)[1] = 0;
        new_inode -> lev3_ptr = 0;
        write_inode(new_inode -> ino);
        //3. create dentry . and ..
        init_dentry(new_inode -> direct_ptr[0], new_inode -> ino, father_dir -> ino);
        //4. add dentry in its parent dir
        bios_sdread(DATA_MEM_ADDR, 8, FS_START + (father_dir -> direct_ptr[0])*8);
        dentry_t *dentry = (dentry_t*)(pa2kva(DATA_MEM_ADDR) + father_dir -> used_size); //find unused addr
        dentry -> mode = DIR;
        dentry -> ino  = new_inode -> ino;
        strcpy(dentry -> name, path);
        bios_sdwrite(DATA_MEM_ADDR, 8, FS_START + (father_dir -> direct_ptr[0])*8);
        //5. update father inode
        father_dir -> mtime = get_timer();
        father_dir -> nlinks ++;
        father_dir -> used_size += 32;
        write_inode(father_dir -> ino);

        printk("Successfully make a directory!\n");    

        return 0;  // do_mkdir succeeds
    }

5）创建文件的步骤

我们可以总结touch的主要步骤
1.检查要创建的文件是否在父目录中已经存在
2.在iNode map中找到一个可用的iNode并进行初始化(这个过程中也找到一个data block存目录项)
3.在父目录中添加这一新创建的目录项
4.更新父目录的inode中相关信息

    int do_touch(char *path)
    {
        // TODO [P6-task2]: Implement do_touch
        int i = 0;
        //Judge if there is a fs
        bios_sdread(SB_MEM_ADDR, 1, FS_START);
        superblock_t *sp = (superblock_t*)(pa2kva(SB_MEM_ADDR));
        if(sp -> magic != SUPERBLOCK_MAGIC){
            printk("Error: No file System!\n");
            return;
        } 
        //check current_node, if not ---> root dir
        bios_sdread(INODE_MEM_ADDR, 1, FS_START + INODE_SD_OFFSET);
        if(state == 0){
            current_inode = (inode_t*)(pa2kva(INODE_MEM_ADDR));
            state = 1;
        }

        //1.Judge if this file has exited
        inode_t *father_inode = current_inode;
        inode_t *find_file = lookup(father_inode, path, FILE);
        if(find_file != NULL){
            printk("This file has already existed!\n");
            return;
        }
        //2.alloc inode and 1 data block
        uint32_t inode_snum = alloc_inode();
        inode_t *new_inode = (inode_t*)(pa2kva(INODE_MEM_ADDR) + inode_snum*512);
        new_inode -> ino = inode_snum;
        new_inode -> mode = FILE;
        new_inode -> access = RW_A;
        new_inode -> nlinks = 1;
        new_inode -> size = 0;
        new_inode -> used_size = 0;//different from dir
        new_inode -> stime = new_inode -> mtime = get_timer();
        new_inode -> direct_ptr[0] = alloc_data_block();
        for(int i = 1; i < DPTR; i++){
            new_inode -> direct_ptr[i] = 0;
        }
        (new_inode -> lev1_ptr)[0] = 0;
        (new_inode -> lev1_ptr)[1] = 0;
        (new_inode -> lev1_ptr)[2] = 0;
        (new_inode -> lev2_ptr)[0] = 0;
        (new_inode -> lev2_ptr)[1] = 0;
        new_inode -> lev3_ptr = 0;
        write_inode(new_inode -> ino);
        //3.adjust its father dir block
        bios_sdread(DATA_MEM_ADDR, 8, FS_START + (father_inode -> direct_ptr[0])*8);
        dentry_t *dentry = (inode_t*)(pa2kva(DATA_MEM_ADDR) + father_inode -> used_size);
        dentry -> mode = FILE;
        dentry -> ino = new_inode -> ino;
        strcpy(dentry -> name, path);
        bios_sdwrite(DATA_MEM_ADDR, 8, FS_START + (father_inode -> direct_ptr[0])*8);
        //4.adjust its father dir inode
        father_inode -> nlinks ++;
        father_inode -> mtime = get_timer();
        father_inode -> used_size += 32;
        write_inode(father_inode -> ino);
        //Finish create a file(like mkdir)
        printk("\nFinish create a file!\n");

        return 0;  // do_touch succeeds
    }


6）文件读写的步骤(以读为例)

我们可以总结fread的主要步骤
1.根据fd中信息找到要读的文件的inode
2.分情况讨论要读的起始位置r_ptr所在的文件的数据块的扇区位置(如果小于10页，直接寻址；如果大于10页，从一级间址寻找起始扇区)
如果要读的大小全部在第一个要读的扇区，则读完更新fd中信息返回；否则将该扇区读完，继续执行。
3.按照读完所在的位置分情况处理，如果读完之后在10页大小内，则只需直接寻址，读完返回；如果读完小于4M+40K但大于40K，则用一级间址的第一个指针；如果读完大于上述小于8M+40K，则用一级间址的第二个指针
4.更新fd中信息返回。

fwrite略微复杂一些，主要思路是一样的，只不过如果写的时候发现数据块没分配要自己alloc数据块，同时每次更新inode内容都要重写inode磁盘。


    int do_fread(int fd, char *buff, int length)
    {
        // TODO [P6-task2]: Implement do_fread
        int i;
        char *temp_buff = buff;
        //Judge if there is a fs
        bios_sdread(SB_MEM_ADDR, 1, FS_START);
        superblock_t *sp = (superblock_t*)(pa2kva(SB_MEM_ADDR));
        if(sp -> magic != SUPERBLOCK_MAGIC){
            printk("Error: No file System!\n");
            return -1;
        } 
        //check current_node, if not ---> root dir
        bios_sdread(INODE_MEM_ADDR, 1, FS_START + INODE_SD_OFFSET);
        if(state == 0){
            current_inode = (inode_t*)(pa2kva(INODE_MEM_ADDR));
            state = 1;
        }
        //get file inode
        inode_t *r_file = get_inode_from_ino(fdesc_array[fd].ino);
        uint32_t r_ptr = fdesc_array[fd].r_ptr;

        //handle unalign(we just use lev1 ptr)
        int sz = length;
        uint8_t *r_buff;
        int pos = r_ptr - 40960;
        if(r_ptr % 512 != 0){
            //< direct ptr 
            if(r_ptr < 40960){
                bios_sdread(DATA_MEM_ADDR, 1, FS_START + (r_file -> direct_ptr[r_ptr/4096])*8 + (r_ptr / 512) % 8);
                r_buff = (uint8_t*)(pa2kva(DATA_MEM_ADDR));
                if(length > (512 - r_ptr % 512)){
                    memcpy(temp_buff, &r_buff[r_ptr % 512], 512 - r_ptr % 512);
                    sz -= (512 - r_ptr % 512);
                    temp_buff += (512 - r_ptr % 512);
                }else{
                    memcpy(temp_buff, &r_buff[r_ptr % 512], length);
                    fdesc_array[fd].r_ptr += length;
                    sz -= length;
                    temp_buff += length;
                    return length;
                }
            }
            //< direct_ptr and lev 1 ptr(lev1_ptr -> 1K*4KB = 4MB), the larget is 4*3 = 12MB
            else{
                //read lev1 block into lev1 addr
                bios_sdread(LEV1_MEM_ADDR, 8, FS_START + (r_file -> lev1_ptr[pos/(1 << 22)])*8);
                uint32_t *lev1 = (uint32_t*)(pa2kva(LEV1_MEM_ADDR));
                //read actual block according to lev1 block
                int pos1 = pos % (1 << 22);
                bios_sdread(DATA_MEM_ADDR, 1, FS_START + lev1[pos1/4096]*8 + (pos1/512)%8);
                r_buff = (uint8_t*)(pa2kva(DATA_MEM_ADDR));
                if(length > (512 - r_ptr % 512)){
                    memcpy(temp_buff, &r_buff[r_ptr%512], 512 - r_ptr % 512);
                    temp_buff += (512 - r_ptr % 512);
                    sz -= (512 - r_ptr % 512);
                }else{
                    memcpy(temp_buff, &r_buff[r_ptr % 512], 512 - r_ptr % 512);
                    fdesc_array[fd].r_ptr += length;
                    sz -= length;
                    temp_buff += length;
                    return length;
                }
            }
        
        
        }
        //i: sector ID
        //If size < 40960
        int ac_size;
        for(i = (r_ptr + 511)/512; i < 80 && sz > 0; i++){
            //We read file every 512B
            bios_sdread(DATA_MEM_ADDR, 1, FS_START + (r_file -> direct_ptr[i/8])*8 + i % 8);
            r_buff = (uint8_t*)(pa2kva(DATA_MEM_ADDR));
            ac_size = (sz > 512)? 512: sz;
            memcpy(temp_buff, r_buff, ac_size);
            temp_buff += ac_size;
            sz   -= ac_size;
        }
        //If size > 40960(32768=1024*8)
        int j;
        if(i >= 80 && sz > 0 && i < 8272){
            bios_sdread(LEV1_MEM_ADDR, 8, FS_START + (r_file -> lev1_ptr[0])*8);
            uint32_t *lev1 = (uint32_t*)(pa2kva(LEV1_MEM_ADDR));
            for(; (i < 8192+80) && sz > 0; i++){
                j = i - 80;
                bios_sdread(DATA_MEM_ADDR, 1, FS_START + lev1[j/8]*8 + j % 8);
                r_buff = (uint8_t*)(pa2kva(DATA_MEM_ADDR));
                ac_size = (sz > 512)? 512:sz;
                memcpy(temp_buff, r_buff, ac_size);
                temp_buff += ac_size;
                sz   -= ac_size;
            }   
        }
        //if size > 40KB + 4MB(TO 40KB + 8MB)
        if(i >= 8272 && sz > 0){
            bios_sdread(LEV1_MEM_ADDR, 8, FS_START + (r_file -> lev1_ptr[1])*8);
            uint32_t *lev1 = (uint32_t*)(pa2kva(LEV1_MEM_ADDR));
            for(; i < (8272+8192) && sz > 0; i++){
                j = i - 8272;
                bios_sdread(DATA_MEM_ADDR, 1, FS_START + lev1[j/8]*8 + j % 8);
                r_buff = (uint8_t*)(pa2kva(DATA_MEM_ADDR));
                ac_size = (sz > 512)? 512:sz;
                memcpy(temp_buff, r_buff, ac_size);
                temp_buff += ac_size;
                sz   -= ac_size;
            }
        }
        fdesc_array[fd].r_ptr += length;

        return (length - sz);
        // return the length of trully read data
    }