#include <assert.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include "bitmap.h"
#include "pages.h"
#include "inode.h"
#include "util.h"
#include "root_list.h"

/*
typedef struct inode
{
    int inum; // index in inode bitmap
    int mode; // permission & type
    int size; // bytes
    int ptrs[NUM_PTRS]; // direct pointers
    int iptr; // single indirect pointer
    struct timespec ts[2]; // last updated time
} inode;
*/

// ATTRIBUTION: Michael Herbert (past CS3650 student)
// helped me understand how to keep track of the inodes with the base
// pointer. base pointer allows me to treat inodes like their own set of blocks

static void* inodes_base = 0;
const int INODE_COUNT = 4096 / sizeof(inode);


//after calling this, inodes_base should be set up
void
inode_init()
{
    // set the page bit for the inodes
    // the inodes should be the first things after the bitmaps
    if (!bitmap_get(get_pages_bitmap(), 1))
    {
        // allocate a page for the inodes
        int page = alloc_page();
        assert(page == 1);
        assert(inodes_base = pages_get_page(page));
    }
    else
    {
        // set the start of the inodes
        inodes_base = pages_get_page(1);
    }
}


inode*
get_inode(int inum)
{
    // max amount of inodes is 4096
    // assert(inum * sizeof(inode) < 4096);
    return (inode*)inodes_base + inum;
}

inode* 
copy_inode(inode* node, int size)
{
    printf("+ copy_inode(%d) -> ... \n", node->inum);
    
    int new_inum = alloc_inode();
    inode* new_node  = get_inode(new_inum);

    
    // this will allocate new page data
    grow_inode(new_node, size);
    
    // this will copy the old direct page(s) data to the new direct page(s) data
    void* page0_old = pages_get_page(node->ptrs[0]);
    void* page0_new = pages_get_page(new_node->ptrs[0]);
    memcpy(page0_new, page0_old, size); // What should size be?

    void* page1_old = pages_get_page(node->ptrs[1]);
    void* page1_new = pages_get_page(new_node->ptrs[1]);
    memcpy(page1_new, page1_old, size); // What should size be?

    //TODO: MAYBE CHANGE THIS
    new_node->iptr = node->iptr;

    new_node->refs = node->refs;
    new_node->mode = node->mode;
    assert(new_node->mode == node->mode);

    new_node->ts[0] = node->ts[0];
    new_node->ts[1] = node->ts[1];

    printf("... end of copy_inode(%d) -> %d\n", node->inum, new_node->inum);

    return new_node;
}

int
find_last_root()
{
    assert(inodes_base);

    void* ibm = get_inode_bitmap();

    for (int ii = 0; ii < INODE_COUNT; ++ii)
    {
        inode* curr = get_inode(ii);
        if(curr->is_root) {
            return ii;
        }
    }

    // return 0;
    return -1;
}

int
alloc_inode()
{
    assert(inodes_base);

    void* ibm = get_inode_bitmap();

    for (int ii = 0; ii < INODE_COUNT; ++ii)
    {
        if (!bitmap_get(ibm, ii))
        {
            bitmap_put(ibm, ii, 1);
            printf("+ alloc_inode() -> %d\n", ii);

            inode* node = get_inode(ii);
            node->inum = ii;
            node->size = 0;
            node->is_root = 0;

            return ii;
        }
    }

    return -1;
}

void
free_inode(inode* node)
{
    printf("+ free_inode(%d)\n", node->inum);
    print_inode(node);
    bitmap_put(get_inode_bitmap(), node->inum, 0);
}

// make the given node have enough blocks to fill the given size
// either through the direct pointers, or by allocating on the indirect pointer
int
grow_inode(inode* node, int size)
{
    printf("+ grow_inode( %d , %d) \n", node->inum, size);

    int blocks_to_grow = bytes_to_pages(size);
    int blocks_on_node = bytes_to_pages(node->size);

    // make the number of blocks this node has
    // match the blocks that are being requested
    while (blocks_on_node < blocks_to_grow)
    {
        if (blocks_on_node < NUM_PTRS)
        {
            //node has a direct block
            node->ptrs[blocks_on_node] = alloc_page();
        }
        else //go to node's indirect blocks
        {
            // haven't used node's indirect block yet
            if (blocks_on_node == NUM_PTRS)
            {
                node->iptr = alloc_page();
            }

            // alloc the next needed block on indirect block
            int* iptrs = (int*)pages_get_page(node->iptr);
            iptrs[blocks_on_node - NUM_PTRS] = alloc_page();
        }

        blocks_on_node++;
    }


    node->size = size;

    print_inode(node);

    return node->size;
}

int
shrink_inode(inode* node, int size) {
    printf("+ shrink_inode( ... , %d) \n", size);
    print_inode(node);

    int blocks_to_shrink = bytes_to_pages(size);
    int blocks_on_node = bytes_to_pages(node->size);

    // make the number of blocks this node has
    // match the blocks that are being requested
    while (blocks_on_node > blocks_to_shrink) {
        free_page(inode_get_pnum(node, blocks_on_node));
        blocks_on_node--;
    }

    // don't need iptrs page anymore, free it
    if (blocks_to_shrink <= NUM_PTRS) {
        free_page(node->iptr);
    }

    node->size = size;
    return node->size;
}

// get the page number that this block points to,
// where fpn is the page number relative to the inode
// direct poitners are 0, 1, indirect are 2,3,... onward
int
inode_get_pnum(inode* node, int fpn)
{
    if (fpn < NUM_PTRS)
    {
        return node->ptrs[fpn];
    }

    return ((int*)pages_get_page(node->iptr))[fpn - NUM_PTRS];
}

void
inode_copy_stats(inode* node, struct stat* st)
{
    st->st_mode = node->mode;
    st->st_size = node->size;
    st->st_atim = node->ts[0];
    st->st_mtim = node->ts[1];
    st->st_uid = getuid();
    st->st_nlink = node->refs; // number of hard links
}

int
inode_is_dir(inode* node)
{
    int mode = (int)node->mode;
    int upper = mode / 512; // 8^3

    //printf("\t\t[DEBUG] is_dir %d: %d reduced -> %d\n", node->inum, mode, upper);
    return (upper == 32);
}

void
print_inode(inode* node)
{

    // printf("node_%d{ptrs:[%d, %d]}, indir:%d\n", node->inum, node->ptrs[0], node->ptrs[1], node->iptr);

    printf("\tnode_%d{\n", node->inum);
    printf("\t\tptrs:[%d, %d], indir:%d\n", node->ptrs[0], node->ptrs[1], node->iptr);
    printf("\t\tmode:%04o, size:%d\n", node->mode, node->size);
    // printf("\t\tts[0]: [%ld, %ld], ts[1]: [%ld, %ld]\n",
    //  node->ts[0].tv_sec, node->ts[0].tv_nsec,
    //  node->ts[1].tv_sec, node->ts[1].tv_nsec);
    printf("\t}\n");
}

