// based on cs3650 starter code

#ifndef INODE_HINTS_H
#define INODE_HINTS_H

#include "pages_HINTS.h"

//inode from hints directory
typedef struct inode_h {
    int refs; // reference count
    int mode; // permission & type
    int size; // bytes
    int ptrs[2]; // direct pointers
    int iptr; // single indirect pointer
} inode_h;

void print_inode(inode_h* node);
inode_h* get_inode(int inum);
int alloc_inode();
void free_inode();
int grow_inode(inode_h* node, int size);
int shrink_inode(inode_h* node, int size);
int inode_get_pnum(inode_h* node, int fpn);

#endif