#include <assert.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include "bitmap.h"
#include "pages.h"
#include "util.h"
#include "slist.h"
#include "root_list.h"

static void* root_list = NULL;

/*
typedef struct cow_version {
    int rnum;
    int vnum;
    char op[26];
} cow_version;
*/

//after calling this, root_list should be set up
void
root_init()
{

    
    // set the page bit for the root_list
    // the root_list should be the first things after the bitmaps
    if (!bitmap_get(get_pages_bitmap(), 2))  {
        // allocate a page for the root list
        int page = alloc_page();
        assert(page == 2);
        assert(root_list = pages_get_page(page));
    }
    else
    {
        // set the root
        root_list = pages_get_page(1); 
    }

    printf("+ root_init() -> %d\n", 2);

    // add_root(0);
}

//gets the inum of the current root
int
get_current_root(){
    return ((cow_version*)root_list)->rnum;
}

//push_back onto root stack
void
add_root(int rnum, char* op)
{

    printf("+ add_root(%d, %s) -> ...\n", rnum, op);

    cow_version* curr = (cow_version*)root_list;
    cow_version* prev = 0;
    int curr_vnum = curr->vnum;

    for (int ii = 7 - 1; ii > 0; --ii) {
        curr = (cow_version*)root_list + ii;
        prev = (cow_version*)root_list + (ii - 1);
        memcpy(curr, prev, sizeof(cow_version*));
    }

    cow_version* add = (cow_version*)root_list;
    add->rnum = rnum;
    add->vnum = curr_vnum + 1;
    strcpy(add->op, op);

    printf("... new version: %d -> \n", add->vnum);
}

slist*
rootlist_version_table(){
    printf("+ rootlist_version_table() -> 7\n");

    slist* build = 0;
    for(int ii = 0; ii < 7; ++ii){
        cow_version* curr = (cow_version*)root_list + ii;
        if(curr && curr->vnum > 0 && strlen(curr->op) > 0) {
            char first[32];
            snprintf(first, sizeof(first), "%d %s", curr->vnum, curr->op);
            build = s_cons(first, build);
        }
    }
    return build;
}