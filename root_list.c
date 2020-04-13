#include <assert.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include "bitmap.h"
#include "pages.h"
#include "util.h"
#include "inode.h"
#include "root_list.h"

inode* root_list[HISTORY_SIZE] = { 0 };

//after calling this, root_list should be set up
void
root_init(int new_disk)
{

    int rnum = 0;
    if(!new_disk) {
        rnum = find_last_root();
    }

    if(!bitmap_get(get_inode_bitmap(), rnum)) {
        assert(alloc_inode() == rnum);

        inode* rootii = get_inode(rnum);
        rootii->mode = 040755;
    }
    else 
    {
        assert(get_inode(rnum)->mode != rnum);
    }
    
    root_list[0] = get_inode(rnum);
    root_list[0]->is_root = 1;

}

inode*
get_root_inode(){
    return get_inode(get_current_root());
}


//gets the inum of the current root
int
get_current_root(){
    return root_list[0]->inum;
}

//push_back onto root stack
void
add_root(inode* rnode) {
    printf("+ add_root(%d)\n", rnode->inum);


    for (int ii = HISTORY_SIZE - 1; ii > 0; --ii) {
        root_list[ii] = root_list[ii - 1];
    }

    root_list[0]->is_root = 0;
    root_list[0] = rnode;
    root_list[0]->is_root = 1;
}