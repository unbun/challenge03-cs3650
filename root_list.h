#ifndef ROOTL_H
#define ROOTL_H

#include "slist.h"

typedef struct cow_version {
    int rnum;
    int vnum;
    char op[24];
} cow_version;

void root_init();
int get_current_root();
void add_root(int rnum, char* op);
slist* rootlist_version_table();
cow_version* get_root_list_idx(int idx);

int traverse_and_free(int rnum, char* op_from_5);
int traverse_and_free_hlp(int rnum, char* path_from_5);

#endif