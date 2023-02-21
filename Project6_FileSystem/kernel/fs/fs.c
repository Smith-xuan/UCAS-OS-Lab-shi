#include <os/string.h>
#include <os/fs.h>
#include <os/kernel.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <os/time.h>


static superblock_t superblock;
static fdesc_t fdesc_array[NUM_FDESCS];

inode_t *current_inode;
int state = 0;

//clear one sector
void clears(uintptr_t mem_addr){
    int i = 0;
    for(i = 0; i < 512; i+=8){
        *((uint64_t*)(mem_addr+i)) = 0;
    }
}

//get a free datablock from blockmap
uint32_t alloc_data_block(){
    bios_sdread(BMAP_MEM_ADDR, 32, FS_START + BMAP_SD_OFFST);
    uint32_t *bm = (uint8_t*)pa2kva(BMAP_MEM_ADDR);
    uint32_t fb = 0;
    for(int i = 0; i < 32*512; i++){
        for(int j = 0; j < 8; j++){
            if(!(bm[i] & (1 << j))){
                fb = 8*i + j;
                //Set bit 1
                bm[i] |= (1 << j);
                bios_sdwrite(BMAP_MEM_ADDR, 32, FS_START + BMAP_SD_OFFST);
                return fb;
            }
        }
    }
    return 0;
}

//clear the blockmap
void clear_data_block(uint32_t block_id){
    bios_sdread(BMAP_MEM_ADDR, 32, FS_START + BMAP_SD_OFFST);
    uint8_t *bm = (uint8_t*)pa2kva(BMAP_MEM_ADDR);
    bm[block_id/8] &= ~(1 << (block_id % 8));
    bios_sdwrite(BMAP_MEM_ADDR, 32, FS_START + BMAP_SD_OFFST);
}

//get a free inode from inodemap
uint32_t alloc_inode(){
    bios_sdread(IMAP_MEM_ADDR, 1, FS_START + IMAP_SD_OFFSET);
    uint8_t *im = (uint8_t*)(pa2kva(IMAP_MEM_ADDR));
    uint32_t ret_inode = 0;
    for(int i = 0; i < 512; i++){
        for(int j = 0; j < 8; j++){
            if(!(im[i] & (1 << j))){
                ret_inode = 8*i + j;
                //set bit 1
                im[i] |= (1 << j);
                bios_sdwrite(IMAP_MEM_ADDR, 1, FS_START + IMAP_SD_OFFSET);
                return ret_inode;
            }
        }
    }
}

//clear the inode map
void clear_inode_map(uint32_t ino){
    bios_sdread(IMAP_MEM_ADDR, 1, FS_START + IMAP_SD_OFFSET);
    uint8_t *im = (uint8_t*)(pa2kva(IMAP_MEM_ADDR));
    im[ino/8] &= ~(1 << (ino % 8));
    bios_sdwrite(IMAP_MEM_ADDR, 1, FS_START + IMAP_SD_OFFSET); 
}

//write a inode into disk
void write_inode(uint32_t ino){
    bios_sdwrite(INODE_MEM_ADDR + ino*512, 1, FS_START + INODE_SD_OFFSET + ino);
}

//Init . and .. for dir
void init_dentry(uint32_t block_num, uint32_t ino, uint32_t pino){
    dentry_t *d = (dentry_t*)(pa2kva(DATA_MEM_ADDR));
    clears(d);
    //. for the dir itself
    d[0].mode = DIR;
    d[0].ino  = ino;
    strcpy(d[0].name, ".");

    //.. for its father dir
    d[1].mode = DIR;
    d[1].ino  = pino;
    strcpy(d[1].name, "..");

    bios_sdwrite(DATA_MEM_ADDR, 8, FS_START + block_num*8);
}


inode_t *get_inode_from_ino(uint32_t ino){
    bios_sdread(INODE_MEM_ADDR + ino*512, 1, FS_START + INODE_SD_OFFSET + ino);
    inode_t *find_inode = (inode_t*)(pa2kva(INODE_MEM_ADDR + ino*512));
    return find_inode;
}

inode_t *lookup(inode_t *parent_dp, char *name, int mode){

    bios_sdread(DATA_MEM_ADDR, 8, FS_START + (parent_dp -> direct_ptr[0])*8);
    dentry_t *d = (dentry_t*)(pa2kva(DATA_MEM_ADDR));

    for(int i = 0; i < parent_dp -> used_size; i += 32){
        if((!strcmp(d[i/32].name, name))){
            inode_t *find_inode = get_inode_from_ino(d[i/32].ino);
            return find_inode;
        }
    }
    return NULL;
}

//to find path, we use recurssion
int find  = 0;
int depth = 0;
int find_dir(char *path, int mode){
    depth = 0;
    int i = 0;
    int j = 0;
    if(depth == 0){
        find = 0;
    }
    /*If absolute path*/
    if(path[0] == '/'){
        bios_sdread(INODE_MEM_ADDR, 1, FS_START + INODE_SD_OFFSET);
        current_inode = (inode_t*)(pa2kva(INODE_MEM_ADDR));
        if(path[1] == '\0'){
            return 1;
        }
        for(i = 0; i < strlen(path); i++){
            path[i] = path[i+1];
        }
        path[i] = '\0';
    }

    char head[20];
    char tail[100];

    for(i = 0; i < strlen(path) && path[i] != '/'; i++){
        head[i] = path[i];
    }
    head[i++] = '\0';
    for(j = 0; i < strlen(path); i++, j++){
        tail[j] = path[i]; 
    }
    tail[j] = '\0';
    
    inode_t *find_inode;
    if((find_inode = lookup(current_inode, head, mode)) != 0){
        depth ++;
        current_inode = find_inode;
        if(tail[0] == '\0'){
            find = 1;
            return 1;
        }
        find_dir(tail, mode);
    }
    if(find  == 1){
        depth --;
        return 1;
    }else{
        depth = 0;
        return 0;
    }
}

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

int do_statfs(void)
{
    // TODO [P6-task1]: Implement do_statfs
    bios_sdread(SB_MEM_ADDR, 1, FS_START);
    superblock_t *sp = (superblock_t*)(pa2kva(SB_MEM_ADDR));
    printk("\n");
    //check sp
    if(sp -> magic != SUPERBLOCK_MAGIC){
        printk(">ERROR: No File System!\n");
        return;
    }
    
    int i,j;
    int used_block = 0;
    int used_inode = 0;
    //used block
    bios_sdread(BMAP_MEM_ADDR, 32, FS_START + BMAP_SD_OFFST);
    uint8_t *bm = (uint8_t*)(pa2kva(BMAP_MEM_ADDR));
    for(i = 0; i < 32*512; i++){
        for(j = 0; j < 8; j++){
            used_block += (bm[i] >> j) & 0x1;
        }
    }
    //used inode
    bios_sdread(IMAP_MEM_ADDR, 1, FS_START + IMAP_SD_OFFSET);
    uint8_t *im = (uint8_t*)(pa2kva(IMAP_MEM_ADDR));
    for(i = 0; i < 512; i++){
        for(j = 0; j < 8; j++){
            used_inode += (im[i] >> j) & 0x1;
        }
    }

    printk("magic: 0x%x\n",sp -> magic);
    printk("used block: %d/%d, start sector: %d(0x%x)\n",used_block, FS_SIZE, FS_START, FS_START);
    printk("inode map offset: %d, occupied sector: %d, used: %d/512\n",sp -> inodemap_start, sp -> inodemap_num, used_inode);
    printk("block map offset: %d, occupied block: %d\n", sp -> blockmap_start, sp -> blockmap_num);
    printk("inode offset: %d, occupied sector: %d\n", sp -> inode_start, sp -> inode_num);
    printk("data offset: %d, occupied sector: %d\n", sp -> datablock_start, sp -> datablock_num);
    printk("inode entry size: %dB, dir entry size: %dB\n", sp -> isize, sp -> dsize);

    return 0;  // do_statfs succeeds
}

int do_cd(char *path)
{
    // TODO [P6-task1]: Implement do_cd
    //check if there is fs
    bios_sdread(SB_MEM_ADDR, 1, FS_START);
    superblock_t *sp = (superblock_t*)(pa2kva(SB_MEM_ADDR));
    if(sp -> magic != SUPERBLOCK_MAGIC){
        printk("File System does not exit!\n");
        return;
    }

    bios_sdread(INODE_MEM_ADDR, 1, FS_START + INODE_SD_OFFSET);
    if(state == 0){
        current_inode = (inode_t*)(pa2kva(INODE_MEM_ADDR));
        state = 1;
    }

    inode_t *temp = current_inode;
    if(path[0] != '\0'){
        if(find_dir(path, DIR) == 0 || current_inode -> mode != DIR){
            current_inode = temp;
            printk(">ERROR: Cannot find this directory");
            return;
        }
    }
    if(current_inode -> mode == FILE){
        current_inode = temp; //cant cd in a file
    }

    return 0;  // do_cd succeeds
}

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

int do_rmdir(char *path)
{
    // TODO [P6-task1]: Implement do_rmdir
    //check fs
    int i;
    bios_sdread(SB_MEM_ADDR, 1, FS_START);
    superblock_t *sp = (superblock_t*)(pa2kva(SB_MEM_ADDR));
    if(sp -> magic != SUPERBLOCK_MAGIC){
        printk("File system does not exit!\n");
        return;
    }

    bios_sdread(INODE_MEM_ADDR, 1, FS_START + INODE_SD_OFFSET);
    if(state == 0){
        current_inode = (inode_t*)(pa2kva(INODE_MEM_ADDR));
        state = 1;
    }

    inode_t *father_inode = current_inode;
    inode_t *find_dir = lookup(father_inode, path, DIR);
    
    //printk("Find_dir  -> ino: %d\n", find_dir -> ino);
    if(find_dir == NULL){
        printk(">ERROR: Cannot find this dir!\n");
        return;
    }
    //clear block map
    clear_data_block(find_dir -> direct_ptr[0]);
    //clear inode map
    clear_inode_map(find_dir -> ino);
    //delete file
    bios_sdread(DATA_MEM_ADDR, 8, FS_START + (father_inode -> direct_ptr[0])*8);
    dentry_t *d = (dentry_t*)(pa2kva(DATA_MEM_ADDR));
    int found1 = 0;
    for(i = 0; i < father_inode -> used_size; i+= 32){
        if(found1){
            memcpy((uint8_t*)(d - 1), (uint8_t*)d, 32);
        }    //?
        if(find_dir -> ino == d -> ino){
            found1 = 1;
        }
        d++;
    }
    bios_sdwrite(DATA_MEM_ADDR, 8, FS_START + (father_inode -> direct_ptr[0])*8);
    //delete all files
    bios_sdread(DATA_MEM_ADDR, 8, FS_START + (find_dir -> direct_ptr[0])*8);
    dentry_t *rm_d = (dentry_t*)(pa2kva(DATA_MEM_ADDR));
    for(i = 0; i < find_dir -> used_size; i+=32){
        if(rm_d[i/32].mode == FILE){
            inode_t *find_file = lookup(father_inode, rm_d[i/32].name, FILE);
            if(find_file != NULL){
                clear_data_block(find_file -> direct_ptr[0]);
                clear_inode_map(find_file -> ino);
            }
        }
    }
    //update father inode
    father_inode -> nlinks --;
    father_inode -> used_size -= 32;
    father_inode -> mtime = get_timer();
    write_inode(father_inode -> ino);
    
    printk("\nFinish delete a directory!\n");

    return 0;  // do_rmdir succeeds
}

int do_ls(char *path, int option)
{
    // TODO [P6-task1]: Implement do_ls
    // Note: argument 'option' serves for 'ls -l' in A-core
    bios_sdread(SB_MEM_ADDR, 1, FS_START);
    superblock_t *sp = (superblock_t*)(pa2kva(SB_MEM_ADDR));
    if(sp -> magic != SUPERBLOCK_MAGIC){
        printk("fs does not exit!\n");
        return;
    }
    bios_sdread(INODE_MEM_ADDR, 1, FS_START + INODE_SD_OFFSET);
    if(state == 0){
        current_inode = (inode_t*)(pa2kva(INODE_MEM_ADDR));
        state = 1;
    }

    if(option == 0){
        inode_t *temp_inode = current_inode;
        if(path != '\0'){
            int found1 = find_dir(path, DIR);
            if(!found1){
                current_inode = temp_inode;
            }
        }

        bios_sdread(DATA_MEM_ADDR, 8, FS_START + (current_inode -> direct_ptr[0])*8);
        dentry_t *d = (dentry_t*)(pa2kva(DATA_MEM_ADDR));
        printk("\n");
        for(int i = 0; i < current_inode -> used_size; i+=32){
            printk("%s ", d[i/32].name);
        }
        printk("\n");
        current_inode = temp_inode;
    }else if(option == 1){
        bios_sdread(DATA_MEM_ADDR, 8, FS_START + (current_inode -> direct_ptr[0])*8);
        dentry_t *d = (dentry_t*)(pa2kva(DATA_MEM_ADDR));
        printk("\n");
        for(int i = 0; i < current_inode -> used_size; i+=32){
            inode_t *file = get_inode_from_ino(d[i/32].ino);
            printk("%s used_size: %dB nlinks: %d ino :%d\n", d[i/32].name, file -> used_size, file -> nlinks, file -> ino);
        }
        printk("\n");
    }


    return 0;  // do_ls succeeds
}

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

int do_cat(char *path)
{
    // TODO [P6-task2]: Implement do_cat
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
    inode_t *father_inode = current_inode;
    int status = find_dir(path, FILE);
    inode_t *find_file;
    if(status == 1){
        find_file = current_inode;
    }else{
        printk("No such File\n");
        return;
    }
    current_inode = father_inode;
    if(find_file == NULL){
        printk("Error: Cannot find this file!\n");
        return;
    }
    char *buf;
    int cnt = 0;
    printk("\n");
    //< 40KB
    int i;
    for(i = 0; i < find_file -> used_size && i < 40960; i += 4096){
        for(int j = 0; j < 8; j++){
            clears(pa2kva(DATA_MEM_ADDR) + j*512);
        }
        bios_sdread(DATA_MEM_ADDR, 8, (find_file -> direct_ptr[i/4096])*8 + FS_START);
        buf = (char*)(pa2kva(DATA_MEM_ADDR));
        for(int k = 0; k < 4096 && cnt < find_file -> used_size; k++, cnt++){
            printk("%c",buf[k]);
        }
    }

    return 0;  // do_cat succeeds
}

int do_fopen(char *path, int mode)
{
    // TODO [P6-task2]: Implement do_fopen
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
    //1. find this file's inode
    inode_t *father_inode = current_inode;
    int status = find_dir(path, FILE);
    inode_t *find_file;
    if(status == 1){
        find_file = current_inode;
    }else{
        printk("No such File\n");
        return;
    }
    current_inode = father_inode;
    if(find_file == NULL){
        printk(">Error: No such File\n");
        return -1;
    }
    //2. check access
    if(find_file -> access != RW_A && find_file -> access != mode){
        printk(">PLV ERROR\n");
        return -1;
    }
    //3. alloc a fd
    int i = 0;
    for(i = 0; i < NUM_FDESCS; i++){
        if(fdesc_array[i].ino == 0){
            break;
        }
    }
    int fd = i;
    fdesc_array[i].access = mode;
    fdesc_array[i].r_ptr = 0;
    fdesc_array[i].w_ptr = 0;
    fdesc_array[i].ino = find_file->ino;
    //4. return the index
    return fd;  
  // return the id of file descriptor
}

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

int do_fwrite(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement do_fwrite
    int i;
    //Judge if there is a fs
    char *temp_buff = buff;
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
    inode_t *w_file = get_inode_from_ino(fdesc_array[fd].ino);
    uint32_t w_ptr = fdesc_array[fd].w_ptr;

    //handle unalign(we just use lev1 ptr) find the w_ptr pos and write the first(or half) block
    uint8_t *w_buff;
    int sz = length;
    int w_pos = w_ptr - 40960;
    if(w_ptr % 512 != 0){
        if(w_ptr < 40960){
            if(w_file -> direct_ptr[w_ptr / 4096] == 0){
                w_file -> direct_ptr[w_ptr / 4096] = alloc_data_block();
                write_inode(w_file -> ino);
                w_file = get_inode_from_ino(w_file -> ino);
            }
            bios_sdread(DATA_MEM_ADDR, 1, FS_START + (w_file -> direct_ptr[w_ptr/4096])*8 + (w_ptr/512)%8);
            w_buff = (uint8_t*)(pa2kva(DATA_MEM_ADDR));
            if(length > (512 - w_ptr % 512)){
                memcpy(&w_buff[w_ptr % 512], temp_buff, 512 - w_ptr % 512);
                sz -= 512 - w_ptr % 512;
                temp_buff += 512 - w_ptr % 512;
                bios_sdwrite(DATA_MEM_ADDR, 1, FS_START + (w_file -> direct_ptr[w_ptr/4096])*8 + (w_ptr/512)%8);
            }else{
                memcpy(&w_buff[w_ptr % 512], temp_buff, length);
                sz -= length;
                temp_buff += length;
                bios_sdwrite(DATA_MEM_ADDR, 1, FS_START + (w_file -> direct_ptr[w_ptr/4096])*8 + (w_ptr/512)%8);
                fdesc_array[fd].w_ptr += length;
                w_file -> used_size = fdesc_array[fd].w_ptr;
                write_inode(w_file -> ino);
                return length;
            }   
        }else{
            if(w_file -> lev1_ptr[w_pos / (1 << 22)] == 0){
                w_file -> lev1_ptr[w_pos / (1 << 22)] = alloc_data_block();
                write_inode(w_file -> ino);
                w_file = get_inode_from_ino(w_file -> ino);
                int clear_buff[1024];
                for(int j = 0; j < 1024; j++){
                    clear_buff[j] = 0;
                }
                bios_sdwrite(kva2pa(clear_buff), 8, FS_START + (w_file -> lev1_ptr[w_pos / (1 << 22)])*8);
            }
            bios_sdread(LEV1_MEM_ADDR, 8, FS_START + (w_file -> lev1_ptr[w_pos/(1 << 22)])*8);
            uint32_t *level1 = (uint32_t*)(pa2kva(LEV1_MEM_ADDR));
            int w_pos1 = w_pos % (1 << 22);
            if(level1[w_pos1/4096] == 0){
               level1[w_pos1/4096] = alloc_data_block();
                bios_sdwrite(kva2pa(level1), 8, FS_START + (w_file -> lev1_ptr[w_pos/(1 << 22)])*8);
                bios_sdread(kva2pa(level1), 8, FS_START + (w_file -> lev1_ptr[w_pos/(1 << 22)])*8);
            }
            bios_sdread(DATA_MEM_ADDR, 1, FS_START + level1[w_pos1/4096]*8 + (w_pos1/512)%8);
            w_buff = (uint8_t*)(pa2kva(DATA_MEM_ADDR));
            if(length > (512 - w_ptr % 512)){
                memcpy(&w_buff[w_ptr % 512], temp_buff, 512 - w_ptr % 512);
                sz -= 512 - w_ptr % 512;
                temp_buff += 512 - w_ptr % 512;
                bios_sdwrite(DATA_MEM_ADDR, 1, FS_START + level1[w_pos1/4096]*8 + (w_pos1/512)%8);
            }else{
                memcpy(&w_buff[w_ptr % 512], temp_buff, length);
                sz -= length;
                temp_buff += length;
                bios_sdwrite(DATA_MEM_ADDR, 1, FS_START + level1[w_pos1/4096]*8 + (w_pos1/512)%8);
                fdesc_array[fd].w_ptr += length;
                w_file -> used_size = fdesc_array[fd].w_ptr;
                write_inode(w_file -> ino);
                return length;
            }
        }
    }
    //write the last blocks
    int ac_size;
    //<10 blocks(all)
    for(i = (w_ptr + 511)/512; i < 80 && sz > 0; i++){
        //Does not alloc block
        if(w_file -> direct_ptr[i / 8] == 0){
            w_file -> direct_ptr[i / 8] = alloc_data_block();
            write_inode(w_file -> ino);
            w_file = get_inode_from_ino(fdesc_array[fd].ino);
        }
        bios_sdread(DATA_MEM_ADDR, 1, FS_START + (w_file -> direct_ptr[i/8])*8 + i % 8);
        w_buff = (uint8_t*)(pa2kva(DATA_MEM_ADDR));
        ac_size = (sz > 512)? 512: sz;
        memcpy(w_buff, temp_buff, ac_size);
        bios_sdwrite(DATA_MEM_ADDR, 1, FS_START + (w_file -> direct_ptr[i/8])*8 + i % 8);
        sz -= ac_size;
        temp_buff += ac_size;
    }
    int j;
    //>10 blocks(all)
    if(i >= 80 && sz > 0 && i < 8272){
        if(w_file -> lev1_ptr[0] == NULL){
            w_file -> lev1_ptr[0] = alloc_data_block();
            write_inode(w_file -> ino);
            w_file = get_inode_from_ino(fdesc_array[fd].ino);
            int clear_buff[1024];
            for(j = 0; j < 1024; j++){
                clear_buff[j] = 0;
            }
            bios_sdwrite(kva2pa(clear_buff), 8, FS_START + (w_file -> lev1_ptr[0])*8);
        }
        bios_sdread(LEV1_MEM_ADDR, 8, FS_START + (w_file -> lev1_ptr[0])*8);
        uint32_t *lev1 = (uint32_t*)(pa2kva(LEV1_MEM_ADDR));
        for(; i < (8192+80) && sz > 0; i++){
            j = i - 80;
            if(lev1[j / 8] == 0){
                lev1[j / 8] = alloc_data_block();
                bios_sdwrite(kva2pa(lev1), 8, FS_START + (w_file -> lev1_ptr[0])*8);
                bios_sdread(kva2pa(lev1), 8, FS_START + (w_file -> lev1_ptr[0])*8);
            }
            bios_sdread(DATA_MEM_ADDR, 1, FS_START + lev1[j/8]*8 + j % 8);   
            w_buff = (uint8_t*)(pa2kva(DATA_MEM_ADDR));
            ac_size = (sz > 512)? 512: sz;
            memcpy(w_buff, temp_buff, ac_size);
            bios_sdwrite(DATA_MEM_ADDR, 1, FS_START + lev1[j/8]*8 + j % 8);
            sz -= ac_size;
            temp_buff += ac_size;
        }
    }
    if(i >= 8272 && sz > 0){
        if(w_file -> lev1_ptr[1] == NULL){
            w_file -> lev1_ptr[1] = alloc_data_block();
            write_inode(w_file -> ino);
            w_file = get_inode_from_ino(fdesc_array[fd].ino);
            int clear_buff[1024];
            for(j = 0; j < 1024; j++){
                clear_buff[j] = 0;
            }
            bios_sdwrite(kva2pa(clear_buff), 8, FS_START + (w_file -> lev1_ptr[1])*8);
        }
        bios_sdread(LEV1_MEM_ADDR, 8, FS_START + (w_file -> lev1_ptr[1])*8);
        uint32_t *lev1 = (uint32_t*)(pa2kva(LEV1_MEM_ADDR));
        for(; i < (8192+8272) && sz > 0; i++){
            j = i - 8272;
            if(lev1[j / 8] == 0){
                lev1[j / 8] = alloc_data_block();
                bios_sdwrite(kva2pa(lev1), 8, FS_START + (w_file -> lev1_ptr[1])*8);
                bios_sdread(kva2pa(lev1), 8, FS_START + (w_file -> lev1_ptr[1])*8);
            }
            bios_sdread(DATA_MEM_ADDR, 1, FS_START + lev1[j/8]*8 + j % 8);   
            w_buff = (uint8_t*)(pa2kva(DATA_MEM_ADDR));
            ac_size = (sz > 512)? 512: sz;
            memcpy(w_buff, temp_buff, ac_size);
            bios_sdwrite(DATA_MEM_ADDR, 1, FS_START + lev1[j/8]*8 + j % 8);
            sz -= ac_size;
            temp_buff += ac_size;
        }
    }
    fdesc_array[fd].w_ptr += length;
    w_file -> used_size = fdesc_array[fd].w_ptr;
    write_inode(w_file -> ino);
    return length;
    // return the length of trully written data
}

int do_fclose(int fd)
{
    // TODO [P6-task2]: Implement do_fclose
    fdesc_array[fd].ino = 0;
    fdesc_array[fd].access = 0;
    fdesc_array[fd].r_ptr = 0;
    fdesc_array[fd].w_ptr = 0;

    return 0;  // do_fclose succeeds
}

int do_ln(char *src_path, char *dst_path)
{
    // TODO [P6-task2]: Implement do_ln
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

    inode_t *father_inode = current_inode;
    int found = find_dir(src_path, FILE);
    inode_t *find_file;
    if(found == 1){
        find_file = current_inode;
    }else{
        printk("No such file!\n");
        return;
    }
    current_inode = father_inode;
    //create a hard link
    //adjust its father dentry
    bios_sdread(DATA_MEM_ADDR, 8, FS_START + (father_inode -> direct_ptr[0])*8);
    dentry_t *dentry = (dentry_t*)(pa2kva(DATA_MEM_ADDR) + father_inode -> used_size);
    dentry -> mode = FILE;
    dentry -> ino = find_file -> ino;
    strcpy(dentry -> name, dst_path);
    bios_sdwrite(DATA_MEM_ADDR, 8, FS_START + (father_inode -> direct_ptr[0])*8);
    //adjust dir inode
    father_inode -> nlinks ++;
    father_inode -> used_size += 32;
    father_inode -> mtime = get_timer();
    write_inode(father_inode->ino);

    find_file -> nlinks ++;
    write_inode(find_file -> ino);
    printk("\nFinish create a hard link\n");

    return 0;  // do_ln succeeds 
}

int do_rm(char *path)
{
    // TODO [P6-task2]: Implement do_rm
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

    inode_t *father_inode = current_inode;
    inode_t *find_file = lookup(father_inode, path, FILE);
    //dentry
    if(find_file == NULL){
        printk("Error: No Such File\n");
        return;
    }
    int found1 = 0;
    bios_sdread(DATA_MEM_ADDR, 8, FS_START + (father_inode -> direct_ptr[0])*8);
    dentry_t *dentry = (dentry_t*)(pa2kva(DATA_MEM_ADDR));
    for(int i = 0; i < father_inode -> used_size; i += 32){
        if(found1){
            memcpy((uint8_t*)(dentry - 1), (uint8_t*)dentry, 32);
        }
        if(!strcmp(path, dentry->name)){
            found1 = 1;
        }
        dentry ++;
    }
    bios_sdwrite(DATA_MEM_ADDR, 8, FS_START + (father_inode -> direct_ptr[0])*8);
    //inode
    father_inode -> used_size -= 32;
    father_inode -> nlinks --;
    father_inode -> mtime = get_timer();
    write_inode(father_inode -> ino);

    find_file -> nlinks --;
    int j = 0;
    if(find_file -> nlinks == 0){
        clear_inode_map(find_file -> ino);
        for(int j = 0; j < 10; j++){
            if(find_file -> direct_ptr[j] != 0){
                clear_data_block(find_file -> direct_ptr[j]);
            }
        }
        if(find_file -> used_size > 40960){
            bios_sdread(LEV1_MEM_ADDR, 8, FS_START + (find_file -> lev1_ptr[0])*8);
            uint32_t *lev1 = (uint32_t*)(pa2kva(LEV1_MEM_ADDR));
            for(int k = 0; k < 1024; k++){
                if(lev1[k] != 0){
                    clear_data_block(lev1[k]);
                }
            }
            clear_data_block(find_file -> lev1_ptr[0]);
        }
        if(find_file -> used_size > ((1 << 22) + 40960) && find_file -> used_size < ((1 << 23) + 40960)){
            bios_sdread(LEV1_MEM_ADDR, 8, FS_START + (find_file -> lev1_ptr[1])*8);
            uint32_t *lev1 = (uint32_t*)(pa2kva(LEV1_MEM_ADDR));
            for(int k = 0; k < 1024; k++){
                if(lev1[k] != 0){
                    clear_data_block(lev1[k]);
                }
            }
            clear_data_block(find_file -> lev1_ptr[1]);
        }
    }else{
        write_inode(find_file -> ino);
    }
    printk("\nFinish delete\n");

    return 0;  // do_rm succeeds 
}

int do_lseek(int fd, int offset, int whence)
{
    // TODO [P6-task2]: Implement do_lseek
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
    inode_t *l_file = get_inode_from_ino(fdesc_array[fd].ino);
    if(whence == SEEK_SET){
        fdesc_array[fd].r_ptr = offset;
        fdesc_array[fd].w_ptr = offset;
    }else if (whence == SEEK_CUR){
        fdesc_array[fd].r_ptr += offset;
        fdesc_array[fd].w_ptr += offset;
    }else if(whence == SEEK_END){
        fdesc_array[fd].r_ptr = l_file -> used_size + offset;
        fdesc_array[fd].w_ptr = l_file -> used_size + offset;
    }

    return fdesc_array[fd].r_ptr;  // the resulting offset location from the beginning of the file
}
