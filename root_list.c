#include <assert.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include "bitmap.h"
#include "pages.h"
#include "util.h"
#include "inode.h"
#include "root_list.h"


/*
struct cow_version {
    int vnum;
    int root_inode_num;
    char note[24];
}
*/

void* roots_base = 0; // the indices are the version numbers
const int ROOT_COUNT = 4096 / sizeof(cow_version);
const int ROOTS_BASE_PAGE = 2;

//after calling this, root_list should be set up
void
root_init(int new_disk)
{
    printf("+ root_init(%d) -> %d\n", new_disk, ROOTS_BASE_PAGE);

    if (!bitmap_get(get_pages_bitmap(), ROOTS_BASE_PAGE))
    {
        // allocate the page for the root list
        int page = alloc_page();
        assert(page == ROOTS_BASE_PAGE);
        assert(roots_base = pages_get_page(page));
    }
    else
    {
        // set the start of the inodes
        roots_base = pages_get_page(ROOTS_BASE_PAGE);
    }
}

cow_version*
get_root(int vnum)
{
    // max amount of inodes is 4096
    // assert(inum * sizeof(inode) < 4096);
    return (cow_version*)roots_base + vnum;
}

int
alloc_root(char* op, char* path)
{
    int new_root = alloc_inode();
    if (new_root < 0) {
        return new_root;
    }

    int vnum = get_current_version() + 1;

    // push the base down one
    cow_version* push_base = (cow_version*)roots_base + 1;
    roots_base = push_base;

    cow_version* new_version = get_root(0);// the new_roots base

    new_version->vnum = vnum;
    new_version->root_inum = new_root;

    char* note = strcat(op, " ");
    note = strcat(note, path);
    strcpy(new_version->note, note);


    assert(roots_base == pages_get_page(2));

    return vnum; // or new_root ???
}

//gets the inum of the current root
int
get_current_root_inum()
{
    return get_root(0)->root_inum;
}

//gets the current version number
int
get_current_version()
{
    return get_root(0)->vnum;
}

void
clean_history(int history_size)
{
    cow_version* not_needed = get_root(history_size);
    free(not_needed);
}