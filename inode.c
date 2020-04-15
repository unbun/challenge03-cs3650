#include <assert.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include "bitmap.h"
#include "pages.h"
#include "inode.h"
#include "util.h"

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


// TODO: best way of version tracking
//     - maybe have a list of these bitmaps, one for each verision?
//     - maybe make a special structure for the root inode
// will probably need to do the same stuff for static pages_fd and pages_bse

#define NUM_DIRECT_PAGES 4

static void* inodes_base = 0;


const int INODE_COUNT = NUM_DIRECT_PAGES * (4096 / sizeof(inode));


//after calling this, inodes_base should be set up
void
inode_init()
{
    // set the page bit for the inodes
    // the inodes should be the first th2ngs after the bitmaps
    if (!bitmap_get(get_pages_bitmap(), 2)) {
        // allocate a page for the
        int page = alloc_page();
        assert(page == 2);
        assert(inodes_base = pages_get_page(page));

        for (size_t ii=0; ii<NUM_DIRECT_PAGES - 1; ++ii) {
            assert(alloc_page() == ii + 3);
        }
    }
    else
    {
        // set the start of the inodes
        inodes_base = pages_get_page(2); 
    }
}


inode*
get_inode(int inum)
{
    // assert(inum >= 0);
    // max amount of inodes is 4096
    int inumspot = (inum * (int)sizeof(inode));
    int inumsize = (4096 * (int)NUM_DIRECT_PAGES);
    int assertMe = inumspot < inumsize;
    if(!assertMe) {
        printf("\t[DEBUG] inum %d out of bounds, %d < %d = %d", inum, inumspot, inumsize, assertMe);
    }
    assert(assertMe);
    return (inode*)inodes_base + inum;
}

/*
    int refs;              // number of references
    int mode;              // permission & type
    int size;              // bytes
    int ptrs[NUM_PTRS];    // direct pointers to the page
    int iptr;              // single indirect pointer

    struct timespec ts[2]; // last updated time
*/

inode*
copy_inode(inode* node)
{
    inode* new_inode = get_inode(alloc_inode());

    printf("+ copy( %d{size=%d) ) ...\n", node->inum, node->size);
    print_inode(node);

    int blocks_to_grow = bytes_to_pages(node->size); // how many 4k pages they are asking for
    int blocks_on_node = 0; // how many 4k pages we have


    // make the number of blocks this node has
    // match the blocks that are being requested
    while (blocks_on_node < blocks_to_grow)
    {
        // if we haven't used all the pages in the direct pointers
        if (blocks_on_node < NUM_PTRS) 
        {
            //node has a direct block
            new_inode->ptrs[blocks_on_node] = alloc_page();
            memcpy(pages_get_page(new_inode->ptrs[blocks_on_node]),
                   pages_get_page(node->ptrs[blocks_on_node]), 4096);
        }
        else //go to node's indirect blocks
        {            
            // haven't used node's indirect block yet
            if (blocks_on_node == NUM_PTRS)
            {
                new_inode->iptr = alloc_page();
            }

            int iptrii = blocks_on_node - NUM_PTRS;

            // alloc the next needed block on indirect block
            int* iptrs = (int*)pages_get_page(new_inode->iptr);
            iptrs[iptrii] = alloc_page();

            int* old_iptrs = (int*)pages_get_page(node->iptr);

            memcpy(pages_get_page(iptrs[iptrii]), 
                   pages_get_page(old_iptrs[iptrii]), 4096);
        }

        blocks_on_node++;
    }

    new_inode->size = node->size;
    new_inode->refs = node->refs;
    new_inode->mode = node->mode;
    new_inode->ts[0] = node->ts[0];
    new_inode->ts[1] = node->ts[1];

    printf("+ end of copy_inode( %d{size=%d} ) -> %d{size=%d}\n",
        node->inum, node->size, new_inode->inum, new_inode->size);

    return new_inode;
}


/*inode* 
copy_inode(inode* node)
{
    printf("+ copy_inode(%d) -> ... \n", node->inum);

    int new_inum = alloc_inode();
    inode* new_node  = get_inode(new_inum);

    new_node->refs = node->refs;
    new_node->mode = node->mode;
    assert(new_node->mode == node->mode);
    assert(new_node->mode != 0);

    grow_inode(new_node, node->size);
    
    memcpy(pages_get_page(new_node->ptrs[0]), pages_get_page(node->ptrs[0]), 4096);

    if(node->size > 4096) {
        memcpy(pages_get_page(new_node->ptrs[1]), pages_get_page(node->ptrs[1]), 4096);
    }

    // the node had iptrs
    // therefore grow_inode allocated an iptr page for new_node
    // ...MAYBE
    if(node->size > 4096 * 2) {
        int ii = 0;
        while(node->iptr[ii] != 0) {
            memcpy(pages_get_page(new_node->iptr[ii]), pages_get_page(node->iptr[ii]), 4096);
        }
    }

    //TODO: MAYBE CHANGE THIS
    //new_node->iptr = node->iptr;

    new_node->ts[0] = node->ts[0];
    new_node->ts[1] = node->ts[1];

    printf("... end of copy_inode(%d) -> %d\n", node->inum, new_node->inum);

    return new_node;
}*/


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

            return ii;
        }
    }

    return -1;
}

void
free_inode(inode* node)
{
    // never free the inodes_base
    assert(node->inum != 0);

    printf("+ free_inode(%d)...\n", node->inum);

    //if(!inode_is_dir(node)){
    shrink_inode(node, 0);
    //}

    print_inode(node);
    bitmap_put(get_inode_bitmap(), node->inum, 0);
    assert(bitmap_get(get_inode_bitmap(), node->inum) == 0);
}

// make the given node have enough blocks to fill the given size
// either through the direct pointers, or by allocating on the indirect pointer
int
grow_inode(inode* node, int size)
{
    printf("+ grow_inode( ... , %d) \n", size);
    print_inode(node);

    int blocks_to_grow = bytes_to_pages(size); // how many 4k pages they are asking for
    int blocks_on_node = bytes_to_pages(node->size); // how many 4k pages we have


    // make the number of blocks this node has
    // match the blocks that are being requested
    while (blocks_on_node < blocks_to_grow)
    {
        // if we haven't used all the pages in the direct pointers
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
    return node->size;
}

int
shrink_inode(inode* node, int size) {
    printf("+ shrink_inode( ... , %d) \n", size);
    print_inode(node);

    int blocks_to_shrink = bytes_to_pages(size);
    int blocks_on_node = bytes_to_pages(node->size);
    //if 0 < size < 4096   -> [n, 0], 0
    //if 4096 < size < 2*4096   -> [n, m], 0
    //if size > 2*4069   -> [n, m] l

    int pages_freed = 0;
    int pnum = 0;
    // make the number of blocks this node has
    // match the blocks that are being requested
    while (blocks_on_node > blocks_to_shrink) {
        //puts("\n\n\n");

        pnum = inode_get_pnum(node, blocks_on_node - 1);

        printf("pnum(%d[%d]): %d\n\n\n", node->inum, blocks_on_node - 1, pnum);

        // avoid 0, 1, 2,, [3, NDP=1]
        //assert(pnum > 2 + NUM_DIRECT_PAGES);

        //0, 1, 2 - (2 + NUM_DIRECT_PAGES) are all special
        if(pnum > (2 + NUM_DIRECT_PAGES)) {
            free_page(pnum);
            pages_freed++;
        }
        
        blocks_on_node--;
    }

    // don't need iptrs page anymore, free it
    //if (blocks_to_shrink <= NUM_PTRS) {
        //free_page(node->iptr);
    //}

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
//    printf("node_%d{mode: %04o, size: %d}\n",
//           node->inum, node->mode, node->size);
    printf("node_%d{ptrs: [%d, %d], indir:%d size: %d}\n",
           node->inum, node->ptrs[0], node->ptrs[1], node->iptr, node->size);
//    printf("node_%d{ts: [%ld, %ld: %ld, %ld]}\n",
//           node->inum, node->ts[0].tv_sec, node->ts[0].tv_nsec,
//           node->ts[1].tv_sec, node->ts[1].tv_nsec);
}

