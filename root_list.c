#include <assert.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include "bitmap.h"
#include "pages.h"
#include "util.h"


int* root_list = NULL;

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
        root_list  = pages_get_page(1); 
    }
}

//gets the inum of the current root
int
get_current_root(){
    return 0;
    // return root_list[0];
}

//push_back onto root stack
void
add_root(int rnum) {
    for (int ii = 0; ii < 6; ++ii) {
        root_list[ii+1] = root_list[ii];
    }
    root_list[0] = rnum;
}