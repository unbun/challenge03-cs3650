
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <alloca.h>
#include <string.h>
#include <libgen.h>
#include <bsd/string.h>
#include <stdint.h>

#include "storage.h"
#include "slist.h"
#include "util.h"
#include "pages.h"
#include "inode.h"
#include "directory.h"
#include "bitmap.h"

//tree lookup but stop before the last item in the file.
int
tree_lookup_stop_early(const char* path)
{
    char* cut_path = strdup(path); // path might be mutating ???
    int inum = tree_lookup(dirname(cut_path));
    free(cut_path);
    return inum;
}

//TODO: refactor/ abstract out some of these functions

void
storage_init(const char* path)
{
    printf("+ storage_init(%s);\n", path);
    pages_init(path); // alloc page 0 for bitmaps
    inode_init();     // should alloc page 1 for inodes if needed

    if (!bitmap_get(get_inode_bitmap(), 0))
    {
        // force root inode
        assert(alloc_inode() == 0);

        inode* root = get_inode(0);
        root->mode = 040755; // dir
    }
    else
    {
        assert(get_inode(0)->mode != 0);
    }
}

int
storage_stat(const char* path, struct stat* st)
{
    printf("+ storage_stat(%s)\n", path);

    // get the node from the path
    int inum = tree_lookup(path);
    if (inum < 0)
        return inum;

    // fill the stats
    inode* node = get_inode(inum);
    inode_copy_stats(node, st);

    return 0;
}

int
storage_read(const char* path, char* buf, size_t size, off_t offset)
{
    int inum = tree_lookup(path);
    if (inum < 0)
        return inum; // should be an error value

    // get the node
    inode* node = get_inode(inum);
    printf("+ storage_read(%s); inode %d\n", path, inum);
    print_inode(node);

    // only read whats in the file
    if(size > node->size - offset)
        size = node->size - offset;

    off_t curr = offset;
    off_t finish = offset + size;

    // fill the buffer
    while (curr < offset + size)
    {
        // end = start of next page or byte after last one we want to read,
        // whichever comes first
        // arithmetic from Michael Herbert
        finish = min((curr + 4096) & (~4095), offset + size);

        char* data = pages_get_page(inode_get_pnum(node, curr / 4096));
        data = data + curr % 4096;
        memcpy(buf, data, finish - curr);

        curr = finish;
        buf += 4096;
    }

    return size;
}

int
storage_write(const char* path, const char* buf, size_t size, off_t offset)
{
    int inum = tree_lookup(path);
    if (inum < 0) {
        return inum; // should be an error value
    }

    // get the node
    inode* node = get_inode(inum);
    printf("+ writing to page: %d\n", inum);

    // grow or shrink the node for writing
    int write_size = offset + size;
    if(node->size < write_size)
    {
        grow_inode(node, write_size);
    }
    else if (node->size > write_size)
    {
        shrink_inode(node, write_size);
    }

    off_t curr = offset;
    off_t finish = offset + size;

    while (curr < offset + size)
    {
        // end = start of next page or byte after last one we want to write,
        // whichever comes first
        // arithmetic from Michael Herbert
        finish = min((curr + 4096) & (~4095), offset + size);

        char* data = pages_get_page(inode_get_pnum(node, curr / 4096));
        data = data + curr % 4096;
        memcpy(data, buf, finish - curr);

        curr = finish;
        buf += 4096;
    }

    return size;
}


int
storage_truncate(const char *path, off_t size)
{
    int inum = tree_lookup(path);
    if (inum < 0)
    {
        return inum; //  should be an error value
    }

    inode* node = get_inode(inum);

    // resize node but not more than needed
    if (node->size < size)
    {
        off_t curr = node->size;
        grow_inode(node, size); // node can be too big

        off_t finish;

        // go through everything after the size
        while (curr < size)
        {
            finish = min((curr + 4096) & (~4095), size);
            char* data = pages_get_page(inode_get_pnum(node, curr / 4096));
            data = data + curr % 4096;

            // override the extra with 0/NULL
            memset(data, 0, finish - curr);
            curr = finish;
        }
    }
    else if (node->size > size)
    {
        shrink_inode(node, size);
    }

    return 0;
}

int
storage_mknod(const char* path, int mode)
{
    if (path[0] != '/')
    {
        return -ENOENT;
    }
    int rv = 0;

    //get the directory
    int dir_inum = tree_lookup_stop_early(path);
    if (dir_inum < 0)
    {
        rv = dir_inum;
    }
    else
    {
        inode* dd = get_inode(dir_inum);

        char* local_path = strdup(path); // get a local copy of the constant char
        char* file = basename(local_path); // the file or base directory
        if (directory_lookup(dd, file) > 0)
        {
            rv = -EEXIST;
        }
        else
        {
            // create new inode for the directory or file
            int inum = alloc_inode();
            get_inode(inum)->mode = mode;
            get_inode(inum)->refs = 1;
            rv = directory_put(dd, file, inum);
        }

        free(local_path);
    }

    return rv;
}

int
storage_unlink(const char* path)
{
    int dir_inum = tree_lookup_stop_early(path);
    if (dir_inum < 0)
    {
        return dir_inum;
    }

    int rv = 0;

    char* local_path = strdup(path);  // get a local copy of the constant char
    char* file = basename(local_path);  // the file or base directory
    int inum = directory_lookup(get_inode(dir_inum), file);
    if (inum < 0)
    {
        rv = inum;
    }
    else
    {
        //get the file or directory
        int path_inum = tree_lookup(path);
        inode* node = get_inode(path_inum);
        // we will delete it
        --node->refs;

        // directory will update its entries and links
        rv = directory_delete(get_inode(dir_inum), file);
    }

    free(local_path);
    return rv;
}

int
storage_link(const char* from, const char* to)
{
    ////////////////////// FROM inum ///////////////////
    // soft link or directory (are virtually the same thing)
    int s_from_inum = tree_lookup_stop_early(from);
    if (s_from_inum < 0)
    {
        return s_from_inum; // directory does not exist
    }

    char* from_local_path = strdup(from);
    char* from_file = basename(from_local_path);

    // hard link
    int h_from_inum = directory_lookup(get_inode(s_from_inum), from_file);

    if (h_from_inum < 0)
    {
        return h_from_inum; // should be an error code
    }

    ////////////////////// To inum ///////////////////
    // soft link or directory (are virtually the same thing)
    int s_to_inum = tree_lookup_stop_early(to);
    if (s_to_inum < 0)
    {
        return s_to_inum; // directory does not exist
    }

    char* to_local_path = strdup(to);
    char* to_file = basename(to_local_path);

    // hard link
    int to_inum = directory_lookup(get_inode(s_to_inum), to_file);

    int rv = 0;
    if (to_inum < 0)
    {
        // to does not exist, so make it as a directory entry
        inode* to_dir = get_inode(s_to_inum);
        rv = directory_put(to_dir, to_file, h_from_inum);

        // update from's references
        inode* from_node = get_inode(h_from_inum);
        ++from_node->refs;
    }
    else
    {
        // to target already exists
        return -EEXIST;
    }

    free(from_local_path);
    free(to_local_path);
    return rv;
}

int
storage_rename(const char* path, const char* to)
{
    // Should be similar to link

    int dir_inum = tree_lookup_stop_early(path);
    if (dir_inum < 0)
    {
        return dir_inum;
    }

    int rv = 0;

    char* local_path = strdup(path);
    char* file = basename(local_path);

    int inum = directory_lookup(get_inode(dir_inum), file);

    if (inum < 0)
    {
        rv = inum;
    }
    else
    {
        // found existing item
        int dir_inum_to = tree_lookup_stop_early(to);
        if (dir_inum_to < 0)
        {
            rv = dir_inum_to;
        }
        else
        {
            char* local_to = strdup(to);
            char* to_file = basename(local_to);

            // make it as a directory entry
            rv = directory_put(get_inode(dir_inum_to), to_file, inum);
            directory_delete(get_inode(dir_inum), file);
            free(local_to);
        }
    }

    free(local_path);
    return rv;
}


int
storage_set_time(const char* path, const struct timespec ts[2])
{
    int dir_inum = tree_lookup_stop_early(path);
    if (dir_inum < 0)
        return dir_inum;


    int inum = tree_lookup(path);
    if (inum < 0)
        return inum;

    //TODO: Idk why I would need to update the struct twice for the memcpy to show up in gdb

    if (inum != 0) // updating twice for the root breaks the mount sometimes
        memcpy(get_inode(dir_inum)->ts, ts, 2 * sizeof(struct timespec));

    memcpy(get_inode(inum)->ts, ts, 2 * sizeof(struct timespec));
    return 0;
}

slist*
storage_list(const char* path)
{
    // i love this one
    return directory_list(path);
}
