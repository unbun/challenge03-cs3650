#include <assert.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <libgen.h>

#include "bitmap.h"
#include "pages.h"
#include "util.h"
#include "slist.h"
#include "inode.h"
#include "directory.h"
#include "root_list.h"

static void* root_list = NULL;

/*
typedef struct cow_version {
    int rnum;
    int vnum;
    char op[26];
} cow_version;
*/

//makes sure data did not get corrupted in root_list
void root_list_sanity_check() {
    for(int ii = 6; ii >= 0; --ii) {
        cow_version* curr = get_root_list_idx(ii);
        if (curr != 0) {
            assert(curr->vnum < 10000);
            assert(curr->rnum < 10000);
        }
    }
}

//after calling this, root_list should be set up
void
root_init()
{

    
    // set the page bit for the root_list
    // the root_list should be the first things after the bitmaps
    if (!bitmap_get(get_pages_bitmap(), 1))  {
        // allocate a page for the root list
        int page = alloc_page();
        assert(page == 1);
        assert(root_list = pages_get_page(page));
        memset(root_list, 0, 7 * sizeof(cow_version*));
    }
    else
    {
        // set the root
        root_list = pages_get_page(1); 
        root_list_sanity_check();
    }

    printf("+ root_init() -> %d\n", 1);

    // add_root(0);
}

//gets the inum of the current root
int
get_current_root(){
    return ((cow_version*)root_list)->rnum;
}

cow_version*
get_root_list_idx(int idx){
    return (cow_version*)root_list + ((idx) * sizeof(cow_version*));
}

//push_back onto root stack
void
add_root(int rnum, char* op)
{

    printf("+ add_root(%d, %s) -> ...\n", rnum, op);

    cow_version* curr = (cow_version*)root_list;
    cow_version* prev = 0;
    int curr_vnum = curr->vnum;

    // garbage collection
    if(curr_vnum > 6){
        traverse_and_free(get_root_list_idx(6)->rnum, 
            get_root_list_idx(5)->op);
    }
    
    for (int ii = 6; ii > 0; --ii) {
        curr = get_root_list_idx(ii); //(cow_version*)root_list + ((ii) * sizeof(cow_version));
        prev = get_root_list_idx(ii-1); //(cow_version*)root_list + ((ii - 1) * sizeof(cow_version));
        memcpy(curr, prev, sizeof(cow_version));
    }

    cow_version* add = (cow_version*)root_list;
    add->rnum = rnum;
    add->vnum = curr_vnum + 1;
    strcpy(add->op, op);
    root_list_sanity_check();

    printf("... new version: %d -> \n", add->vnum);    
}

int
traverse_and_free(int rnum, char* op_from_5)
{
    slist* sl = s_split(op_from_5, ' ');
    char* path = strdup(sl->next->data);
    s_free(sl);
    int rv = traverse_and_free_hlp(rnum, path);
    printf("+ travers_and_free(%d, %s) -> %d", rnum, path, rv);
    return rv;

}

int
traverse_and_free_hlp(int rnum, char* path_from_5)
{
    //remove me from path   
    //get inode for directory,
    int inum = tree_lookup_hlp(path_from_5, rnum);

    if(inum <= 0){
        return inum;
    }

    inode* me = get_inode(inum);

    free_inode(me);

    if(streq(path_from_5, "/")){
        return 0;
    }

    char* pathdup = strdup(path_from_5);
    traverse_and_free_hlp(rnum, dirname(pathdup));
    free(pathdup);
}

slist*
rootlist_version_table(){
    printf("+ rootlist_version_table() -> 7\n");

    slist* build = 0;
    // int size = min(7, get_root_list_idx(0)->vnum);
    
    // build this list backwards so that it's in the right order for printing
    for(int ii = 6; ii >= 0; --ii){
        cow_version* curr = get_root_list_idx(ii); //(cow_version*)root_list + ii * sizeof(cow_version);
        char first[32];
        snprintf(first, sizeof(first), "%d %s", curr->vnum, curr->op);
        build = s_cons(first, build);
    }
    root_list_sanity_check();
    return build;
}