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

#define NUM_VESRIONS 7


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
    for(int ii = NUM_VESRIONS - 1; ii >= 0; --ii) {
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
        memset(root_list, 0, NUM_VESRIONS * sizeof(cow_version*));
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
    if(curr_vnum > NUM_VESRIONS - 1){
        cow_version* to_free = get_root_list_idx(NUM_VESRIONS - 1);
        cow_version* replacable = get_root_list_idx(NUM_VESRIONS - 2);
        int rv = traverse_and_free(to_free, replacable); 

        if(rv < 0) {
            printf("\t[DEBUG] traverse and free failed! freeing: {%d, %d} with {%d, %d): %d",
                to_free->vnum, to_free->rnum, replacable->vnum, replacable->rnum, rv);
        }
    }
    
    for (int ii = NUM_VESRIONS - 1; ii > 0; --ii) {
        curr = get_root_list_idx(ii); //(cow_version*)root_list + ((ii) * sizeof(cow_version));
        prev = get_root_list_idx(ii-1); //(cow_version*)root_list + ((ii - 1) * sizeof(cow_version));
        memcpy(curr, prev, sizeof(cow_version));
    }

    cow_version* add = (cow_version*)root_list;
    add->rnum = rnum;
    add->vnum = curr_vnum + 1;
    strcpy(add->op, op);
    root_list_sanity_check();

    printf("... new version: -> %d \n", add->vnum);    
}

//CHANGE THIS TO ACCOUNT FOR MKNOD AND OTHER OPERATIONS
int
traverse_and_free(cow_version* to_free, cow_version* next_ver)
{
    int rnum6 = to_free->rnum;
    char* label_from_6 = to_free->op;
    char* label_from_5 = next_ver->op;

    slist* sl6 = s_split(label_from_6, ' '); // syscall from 6->op
    char* syscall6 = strdup(sl6->data);
    char* path6 = strdup(sl6->next->data);

    slist* sl5 = s_split(label_from_5, ' '); // path from 5->op
    char* syscall5 = strdup(sl5->data);
    char* path5 = strdup(sl5->next->data);

    s_free(sl5);
    s_free(sl6);

    if(streq(syscall6, "init") || streq(syscall5, "init")){
        return 0;
    }

    int rv = 0;
    if (streq(syscall5, "mknod"))
    {
        rv = traverse_and_free_hlp(rnum6, dirname(path5));
    }
    
    if (streq(syscall5, "write")) {
        rv = traverse_and_free_hlp(rnum6, path5);
    }

    if (streq(syscall5, "truncate")) {
        rv = traverse_and_free_hlp(rnum6, path5);
    }

    printf("+ traverse_and_free ({%s %s r=%d}, {%s %s}) -> %d\n", syscall6, path6, rnum6, syscall5, path5, rv);

    free(syscall5);
    free(syscall6);
    free(path6);
    free(path5);

    return rv;

}

int
traverse_and_free_hlp(int rnum6, char* path_from_5)
{
    //remove me from path   
    //get inode for directory,
    int inum = tree_lookup_hlp(path_from_5, rnum6);

    // don't free the inodes_base
    if (inum == 0) {
        return inum;
    }

    if(inum < 0){
        // assert(0);
        return inum;
    }

    

    inode* me = get_inode(inum);

    // bitmap_print(get_inode_bitmap(), 512);
    free_inode(me);
    // bitmap_print(get_inode_bitmap(), 512);

    if(streq(path_from_5, "/")){
        return 0;
    }

    char* pathdup = strdup(path_from_5);
    traverse_and_free_hlp(rnum6, dirname(pathdup));
    free(pathdup);
    return 0;
}

slist*
rootlist_version_table(){
    printf("+ rootlist_version_table() -> 7\n");

    slist* build = 0;
    // int size = min(7, get_root_list_idx(0)->vnum);
    
    // build this list backwards so that it's in the right order for printing
    for(int ii = NUM_VESRIONS - 1; ii >= 0; --ii){
        cow_version* curr = get_root_list_idx(ii); //(cow_version*)root_list + ii * sizeof(cow_version);

        char first[32];
        // snprintf(first, sizeof(first), "%d %s", curr->vnum, curr->op);
        snprintf(first, sizeof(first), "%d %s {r=%d}", curr->vnum, curr->op, curr->rnum);

        build = s_cons(first, build);
    }
    root_list_sanity_check();
    return build;
}