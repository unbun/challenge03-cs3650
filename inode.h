// based on cs3650 starter code

#ifndef INODE_H
#define INODE_H

#include <sys/stat.h>
#include <time.h>

#include "pages.h"

#define NUM_PTRS 2

typedef struct inode
{
    int inum;              // index in inode bitmap. see attribution in inodes.c

    int refs;              // number of references
    int mode;              // permission & type
    int size;              // bytes
    int ptrs[NUM_PTRS];    // direct pointers
    int iptr;              // single indirect pointer

    struct timespec ts[2]; // last updated time
} inode;

void inode_init();
void print_inode(inode* node);
inode* get_inode(int inum);
int alloc_inode();
void free_inode(inode* node);
int grow_inode(inode* node, int size);
int shrink_inode(inode* node, int size);
int inode_get_pnum(inode* node, int fpn);

void inode_copy_stats(inode* node, struct stat* st);

#endif
