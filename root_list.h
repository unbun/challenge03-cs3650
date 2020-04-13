#ifndef ROOTL_H
#define ROOTL_H

#include "inode.h"

#define HISTORY_SIZE 7

void root_init(int new_disk);

int get_current_root();
inode* get_root_inode();

void add_root(inode* rnode);

#endif