// based on cs3650 starter code

#ifndef DIRECTORY_HINTS_H
#define DIRECTORY_HINTS_H

#define DIR_NAME 48

#include "slist.h"
#include "pages.h"
#include "inode.h"

//dirent is already defined in POSIX
typedef struct ddirent {
    char name[DIR_NAME];
    int  inum;
    char _reserved[12];
} ddirent;

int directory_lookup(inode* dd, const char* name); // get inum of object name in the dd
int tree_lookup(const char* path);
int directory_put(inode* dd, const char* name, int inum);
int directory_delete(inode* dd, const char* name);
slist* directory_list(const char* path);
void print_directory(inode* dd);

ddirent* directory_get_entries(inode* dd);

#endif

