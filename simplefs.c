#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "simplefs.h"
#include <string.h>


struct sb_info{
    int file_count;
    int dir_count;
};
struct fat_entry{
    int used;
    int next;
};
struct dir_entry{
    char name[32];
    int fb;    // first block
    int used;
    int size;
};
struct open_file_entry{
    char name[32];
    int mode;
    int pos;
};
int vdisk_fd; // global virtual disk file descriptor
              // will be assigned with the sfs_mount call
              // any function in this file can use this.

int root_offset, fat_offset, file_data;
int fatsize = 131072;

int sdir = sizeof(struct dir_entry);
int sfat = sizeof(struct fat_entry);
int ssb = sizeof(struct sb_info);
int sofe = sizeof(struct open_file_entry);

struct sb_info* sbinit;
struct dir_entry* root;
struct fat_entry* fat;
struct open_file_entry* openfiles;



// This function is simply used to a create a virtual disk
// (a simple Linux file including all zeros) of the specified size.
// You can call this function from an app to create a virtual disk.
// There are other ways of creating a virtual disk (a Linux file)
// of certain size. 
// size = 2^m Bytes
int create_vdisk (char *vdiskname, int m)
{
    char command[BLOCKSIZE]; 
    int size;
    int num = 1;
    int count; 
    size  = num << m;
    count = size / BLOCKSIZE;
    printf ("%d %d\n", m, size);
    sprintf (command, "dd if=/dev/zero of=%s bs=%d count=%d",
	     vdiskname, BLOCKSIZE, count);
    printf ("executing command = %s\n", command); 
    system (command); 
    return (0); 
}



// read block k from disk (virtual disk) into buffer block.
// size of the block is BLOCKSIZE.
// space for block must be allocated outside of this function.
// block numbers start from 0 in the virtual disk. 
int read_block (void *block, int k)
{
    int n;
    int offset;

    offset = k * BLOCKSIZE;
    printf("reading offset: %d\n",offset);
    lseek(vdisk_fd, (off_t) offset, SEEK_SET);
    n = read (vdisk_fd, block, BLOCKSIZE);
    if (n != BLOCKSIZE) {
        printf ("read error\n");
        return -1;
    }
    return (0); 
}

// write block k into the virtual disk. 
int write_block (void *block, int k, int size)
{
    if( size > BLOCKSIZE )
        size = BLOCKSIZE;
    int n;
    int offset;

    offset = k * BLOCKSIZE;
    lseek(vdisk_fd, (off_t) offset, SEEK_SET);
    n = write (vdisk_fd, block, size);
    if (n != size) {
        printf ("write error -- %d, off=%d\n",n,offset);
        perror("asd");
        return (-1);
    }
    return 0; 
}

int swrite(int fd, void* buf, int offset, int size){
    lseek(fd, offset, SEEK_SET);
    return write(fd, buf, size);
}

void sread(int fd, void* buf, int offset, int size){
    lseek(fd, offset, SEEK_SET);
    read(fd, buf, size);
}

/**********************************************************************
   The following functions are to be called by applications directly. 
***********************************************************************/

int eq( char *filex, char *filey ) {
    int same = 1;
    for(int j=0;j<32;j++)
        same &= (filey[j] == filex[j]);
    return same;
}

int getroot( char *filename ) {
    for(int i=0;i<56;i++) {
        if( eq( filename, root[i].name ) )
            return i;
    }
    return -1;
}

int updateFat() {
    for(int i=8;i<1032;i++)
        swrite( vdisk_fd, fat+(i-8)*BLOCKSIZE, i*BLOCKSIZE, BLOCKSIZE );
}

int sfs_format (char *vdiskname)
{
    // populate superblock
    int vd = open(vdiskname, O_RDWR); 
    sbinit = malloc(sizeof(struct sb_info));
    sbinit->file_count = 0;
    sbinit->dir_count = 1;
    swrite(vd, sbinit, 0, sizeof(struct sb_info));

    // init root, fat, file_data offsets
    root_offset = BLOCKSIZE;
    fat_offset = BLOCKSIZE*8;
    file_data = BLOCKSIZE*1032;
    
    // init fat entries
    fat = malloc(sfat * fatsize);
    struct fat_entry* initfat = malloc(sfat);
    initfat->used = 0;
    initfat->next = -1;
    for(int i = fat_offset; i < BLOCKSIZE*1032; i+=8){
        swrite(vd, initfat, i, sfat);
        if(i!=131072)
        fat[(i-fat_offset)/8] = *initfat;
    }
    // init root entries
    root = malloc(sdir*56);
    struct dir_entry* initdir = malloc(sdir);
    initdir->used = 0;
    initdir->size = 0;
    initdir->fb = -1;
    for(int i = root_offset; i < BLOCKSIZE*8; i+=128){
        swrite(vd, initdir, i, sdir);
        root[(i-root_offset)/128] = *initdir;
    }
    

    close(vd);

    openfiles = malloc(sofe * 56);
    struct open_file_entry* ofe = malloc(sofe);
    ofe->pos = 0;
    memcpy(ofe->name, "", 1);
    ofe->mode = -1;
    for(int i = 0; i < 56; i++){
        openfiles[i] = *ofe;
    }

    return (0); 
}

int sfs_mount (char *vdiskname)
{
    // simply open the Linux file vdiskname and in this
    // way make it ready to be used for other operations.
    // vdisk_fd is global; hence other function can use it. 
    vdisk_fd = open(vdiskname, O_RDWR); 
    return(0);
}

int sfs_umount ()
{
    fsync (vdisk_fd); 
    close (vdisk_fd);
    return (0); 
}


int sfs_create(char *filename)
{
    if(sbinit->file_count >= 56){
        printf("Max number of files is reached\n");
        return -1;
    }
    struct dir_entry* file_record = malloc(sdir);
    for(int i = 0; i < fatsize; i++){                                       // find a fat entry
        if(!fat[i].used){                                                   
            fat[i].used = 1;                                                // mark as used
            printf("fat found i: %d\n", i);
            file_record->fb = i;                                            // set file record
            file_record->used = 1;
            memcpy(file_record->name, filename, strlen(filename)+1);
            swrite(vdisk_fd, &(fat[i]), fat_offset + i*sfat, sfat);
            break;
        }
    }
    if(!file_record->used){
        printf("sfs_create no blocks are available\n");
        return -1;
    }
    
    for(int i = 0; i < 56; i++){                                            // find unused directory entry
        if(!root[i].used){
            root[i] = *file_record;                                         // set directory entry to file_record
            swrite(vdisk_fd, file_record, root_offset + i*sdir, sdir);      // write into vdisk
            break;
        }   
    }

    sbinit->file_count++;                                                   // update superblock info
    swrite(vdisk_fd, sbinit, 0, ssb);
    return (0);
}


int sfs_open(char *file, int mode)
{
    int empty = 56;
    for(int i = 0; i < 56; i++){
        if(strlen(openfiles[i].name) == 0 && empty > i){
            empty = i;
        }
        if(eq(openfiles[i].name, file)){
            printf("file %s is already open in mode %d", file, openfiles[i].mode);
            return -1;
        }
    }

    memcpy(openfiles[empty].name, file, strlen(file)+1);
    openfiles[empty].mode = mode;

    return (empty); 
}

int sfs_close(int fd){
    openfiles[fd].mode = -1;
    openfiles[fd].pos = 0;
    memcpy(openfiles[fd].name, "", 1);
    updateFat();
    return (0); 
}

int sfs_getsize (int  fd)
{
    for(int i = 0; i < 56; i++){
        if(eq(root[i].name, openfiles[fd].name)){
            return root[i].size;
        }
    }
    printf("file with fd %d is not open\n", fd);
    return (-1); 
}

int sfs_read(int fd, void *buf, int n){
    if(openfiles[fd].mode != 0){
        printf("sfs_read problem mode %d\n", openfiles[fd].mode);
        return -1;
    }
    struct open_file_entry of = openfiles[fd];
    
    struct dir_entry f = root[getroot( of.name )];
    printf("READING FROM: FD=%d\n",getroot(of.name));
    int curfat = root[fd].fb;
    int bytes_to_read = f.size < n ? f.size : n;
    int rem = bytes_to_read;
    int block_no = f.size%1024 == 0 ? f.size/1024 : f.size/1024+1;
    int poscpy = of.pos;
    printf("--- %d, %d\n",curfat,poscpy);
    while(poscpy > 1024){               // come to pos's block
        curfat = fat[curfat].next;
        poscpy -= 1024;
    }

    void* block_read;
    int bytes_read = 0;
    while(rem){
        int read_size = rem < 1024 ? rem : 1024;
        printf("---- cur=%d, %d, sz=%d\n",curfat,(curfat+1032)*1024,read_size);
        sread(vdisk_fd, block_read, (curfat+1032)*1024, read_size);
        printf("ok %d %d\n",bytes_read,read_size);
        char *hmm = block_read;
        printf("muahha %d\n",hmm[0]);
        printf("ok %d %d\n",bytes_read,read_size);
        memcpy(buf+bytes_read, block_read, read_size);printf("ok\n");
        
        bytes_read += read_size;
        printf("ok\n");
        rem -= read_size;
        of.pos += read_size;
        printf("ok\n");
        curfat = fat[curfat].next;
    }
    printf("pos after: %d\n", of.pos);
    printf("bytes_to_read: %d, bytes_read: %d, rem: %d\n", bytes_to_read, bytes_read, rem);

    return (bytes_to_read); 
}

int getfree() {
    for(int i=0;i<fatsize;i++)
        if( !fat[i].used )
            return i;
    return -1;
}

int sfs_append(int fd, void *buf, int n)
{
    fd = getroot(openfiles[fd].name);
    int block = root[fd].fb;
    int sz = root[fd].size;

    root[fd].size += n;
    while( sz > 1024 ) {
        sz -= 1024;
        block = fat[block].next;
    }
    int add = 1024-sz;
    if( n < add ) add = n;
    int offset = (block+1032)*BLOCKSIZE + sz;
    printf("writing----> FD=%d, block=%d(sz=%d), offset=%d -- add=%d\n",fd,block,sz,offset,add);
    swrite( vdisk_fd, buf, offset, add );
    buf += add;
    n -= add;
    while( n > 0 ) {
        fat[block].next = getfree();
        block = fat[block].next;
        if( block == -1 )
            return -1;
        fat[block].used=1;
        //printf("block/writing----> block=%d -- n=%d\n",block,n);
        write_block( buf, block+1032, n );
        n -= 1024;
        buf += 1024;
    }
    return (0);
}

int sfs_delete(char *filename)
{
    int fd = getroot(filename);
    if( fd == -1 )
        return fd;
    int block = root[fd].fb;
    int sz = root[fd].size;
    while( sz > 1024 ) {
        sz -= 1024;
        int h = block;
        block = fat[block].next;
        fat[h].next = -1;
        fat[h].used = 0;
    }
    return (0); 
}