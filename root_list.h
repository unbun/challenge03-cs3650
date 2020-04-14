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

#endif