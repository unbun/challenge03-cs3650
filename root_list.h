#ifndef ROOTL_H
#define ROOTL_H

#include "inode.h"

#define HISTORY_SIZE 7

typedef struct cow_version {
    int vnum;
    int root_inum; // the inode of the root for this version
    char note[24];
} cow_version;

void root_init(int new_disk);
cow_version* get_root(int vnum); // uses version number
int alloc_root(char* op, char* path); // returns new version number
int get_current_root_inum();
int get_current_version();
void clean_history();

#endif