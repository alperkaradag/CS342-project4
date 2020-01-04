#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "simplefs.h"

#define DISKNAME "vdisk1.bin"

int main()
{
    int ret;
    int fd1, fd2, fd; 
    int i; 
    char buffer[1024];
    char buffer2[8] = {50, 50, 50, 50, 50, 50, 50, 50};
    int size;
    char c; 

    printf ("started\n"); 
    
    // ****************************************************
    // if this is the first running of app, we can
    // create a virtual disk and format it as below
    ret  = create_vdisk (DISKNAME, 24); // size = 16 MB
    if (ret != 0) {
	printf ("there was an error in creating the disk\n");
	exit(1); 
    }
    ret = sfs_format (DISKNAME);
    printf("ASFHASF\n");
    if (ret != 0) {
	printf ("there was an error in format\n");
	exit(1); 
    }
    // ****************************************************

    ret = sfs_mount (DISKNAME); 
    printf("ASFHASF\n");
    if (ret != 0) {
	printf ("could not mount \n");
	exit (1); 
    }

    printf ("creating files\n"); 
    sfs_create ("file1.bin");
    sfs_create ("file2.bin");
    sfs_create ("file3.bin");

    fd1 = sfs_open ("file1.bin", MODE_APPEND);
    fd2 = sfs_open ("file2.bin", MODE_APPEND); 
    for (i = 0; i < 10; ++i) {
    	buffer[0] =   (char) 65;  
    	sfs_append (fd1, (void *) buffer, 1);
    }
    for (i = 0; i < 10; ++i) {
    	buffer[0] = (char) 70;
    	buffer[1] = (char) 71;
    	buffer[2] = (char) 72;
    	buffer[3] = (char) 73;
    	sfs_append(fd2, (void *) buffer, 4);
    }

    sfs_close(fd1); 
    sfs_close(fd2); 
    char look[10002];

    fd1 = sfs_open ("file1.bin", MODE_READ);
    fd2 = sfs_open ("file2.bin", MODE_READ); 

    sfs_read(fd2,look,10);
    for(i=0;i<10;i++)
        printf("-- %d\n",look[i]);
    
    sfs_read(fd1,look,10);
    for(i=0;i<10;i++)
        printf("fd1=-- %d\n",look[i]);
    
    sfs_close(fd1); 
    sfs_close(fd2); 
    return 0;
    
    fd = sfs_open("file3.bin", MODE_APPEND);
    for (i = 0; i < 10000; ++i) {
	memcpy (buffer, buffer2, 8); // just to show memcpy
	sfs_append(fd, (void *) buffer, 8); 

    }
    sfs_close (fd); 

    fd = sfs_open("file3.bin", MODE_READ);
    size = sfs_getsize (fd);
    for (i = 0; i < size; ++i) {
	sfs_read (fd, (void *) buffer, 1);
	c = (char) buffer[0];
    }
    sfs_close (fd); 
    
    ret = sfs_umount(); 
}
