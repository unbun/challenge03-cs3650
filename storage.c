
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
#include "root_list.h"


////////// HELPERS ///////////////////////////////////////

//tree lookup but stop before the last item in the file.
int
tree_lookup_stop_early(const char* path)
{
    char* cut_path = strdup(path); // path might be mutating ???
    int inum = tree_lookup(dirname(cut_path));
    free(cut_path);
    return inum;
}


int
traverse_and_update(const char* path, int old_inum, int new_inum, char* op)
{
    if(streq(path, "/")) {
        printf("\t[DEBUG] adding root (%s): %d\n", path, new_inum);
        add_root(new_inum, op);
        return 0;
    }

    //remove me from path   
    //get inode for directory,
    int inum = tree_lookup_stop_early(path);
    if (inum < 0) {
        printf("\t[DEBUG] tree lookup(%s) fail: %d\n", path, inum);
        return inum; // should be an error value
    }

    //  copy directory, add me instead of old inode
    inode* dir = get_inode(inum);
    inode* cow_dir = copy_inode(dir);
    assert(cow_dir->mode == (mode_t)16877 || cow_dir->mode == (mode_t)16893);

    //     get entry of old version (using old_inum)
    //     replace that entry with me (using new_inum)
    replace_in_entries(cow_dir, old_inum, new_inum);

    printf("\t[DEBUG] old inum: %d; new inum: %d\n", old_inum, new_inum);

    char* pathdup = strdup(path);
    int rv = traverse_and_update(dirname(pathdup), dir->inum, cow_dir->inum, op);
    free(pathdup);

    return rv;
    //get inode above me, copy inode above, add me instad of old node
    //if root, do above process, and call add_root()
}

////////// HELPERS ///////////////////////////////////////


//TODO: refactor/ abstract out some of these functions

void
storage_init(const char* path)
{
    // printf("+ storage_init(%s);\n", path);
    pages_init(path); // alloc page 0 for bitmaps
    root_init();      // alloc page 1 for root list 
    inode_init();     // should alloc page 2 for inodes

    // no root created at all
    if (!bitmap_get(get_inode_bitmap(), 0))
    {
        // force root inode
        assert(alloc_inode() == 0);

        char op[24];

        snprintf(op, sizeof(op), "init %s", path);
        add_root(0, op);

        inode* root = get_inode(0);
        root->mode = (mode_t)16877; // dir
    }
    else
    {
        assert(get_inode(get_current_root())->mode != 0);
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

    // TODO: copy the node and work with that cow_node
    inode* cow_node = copy_inode(node);
    printf("+ writing to page: %d\n", inum);


    // grow or shrink the cow_node for writing
    int write_size = offset + size;
    if(cow_node->size < write_size)
    {
        grow_inode(cow_node, write_size);
    }
    else if (cow_node->size > write_size)
    {
        shrink_inode(cow_node, write_size);
    }

    off_t curr = offset;
    off_t finish = offset + size;

    while (curr < offset + size)
    {
        // end = start of next page or byte after last one we want to write,
        // whichever comes first
        // arithmetic from Michael Herbert
        finish = min((curr + 4096) & (~4095), offset + size);

        char* data = pages_get_page(inode_get_pnum(cow_node, curr / 4096));
        data = data + curr % 4096;
        memcpy(data, buf, finish - curr);

        curr = finish;
        buf += 4096;
    }


    char op[24];
    snprintf(op, sizeof(op), "write %s", path);

    // TODO: go up the node's path and update everything to be a new version
    traverse_and_update(path, node->inum, cow_node->inum, op);

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
    inode* cow_node = copy_inode(node);

    // resize node but not more than needed
    if (cow_node->size < size)
    {
        off_t curr = cow_node->size;
        grow_inode(cow_node, size); // node can be too big

        off_t finish;

        // go through everything after the size
        while (curr < size)
        {
            finish = min((curr + 4096) & (~4095), size);
            char* data = pages_get_page(inode_get_pnum(cow_node, curr / 4096));
            data = data + curr % 4096;

            // override the extra with 0/NULL
            memset(data, 0, finish - curr);
            curr = finish;
        }
    }
    else if (cow_node->size > size)
    {
        shrink_inode(cow_node, size);
    }

    int rv = 0;
    char op[24];
    snprintf(op, sizeof(op), "truncate %s", path);

    rv = traverse_and_update(path, node->inum, cow_node->inum, op);

    return rv;
}

int
storage_mknod(const char* path, int mode)
{
    printf("+ mknod (%s, %d)\n", path, mode);

    assert(mode != 0);   
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
        inode* cow_dd = copy_inode(dd);
        assert(cow_dd->mode == (mode_t)16877 || cow_dd->mode == 16893);
        // copy dd -> cow_dd
        
        char* path_lvar = strdup(path); // get a local copy of the constant char
        char* file = basename(path_lvar); // the file or base directory

        if (directory_lookup(cow_dd, file) > 0)
        {
            rv = -EEXIST;
        }
        else
        {
            // create new inode for the directory or file
            int inum = alloc_inode();
            get_inode(inum)->mode = mode;
            get_inode(inum)->refs = 1;

            // use cow_dd instead of dd
            rv = directory_put(cow_dd, file, inum);
        }

        if(rv < 0) {
            return rv;
        }

        // traverse and update the directory's path
        char op[24];
        snprintf(op, sizeof(op), "mknod %s", path);
        rv = traverse_and_update(dirname(path_lvar), dd->inum, cow_dd->inum, op);

        free(path_lvar);
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

    char* path_lvar = strdup(path);  // get a local copy of the constant char
    char* file = basename(path_lvar);  // the file or base directory

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
        inode* cow_node = copy_inode(node);


        // we will delete it
        cow_node->refs = cow_node->refs - 1;

        char op[24];
        snprintf(op, sizeof(op), "unlink %s", path);
        //                       path   old       new             op
        rv = traverse_and_update(path, path_inum, cow_node->inum, op);
        if(rv < 0) {
            return rv;
        }

        // by this point, directory's entries list is updated with the cow node
        // which is reflected by a new copy of the directory
        int cow_dir = tree_lookup_stop_early(path);
        if (dir_inum < 0)
        {
            return dir_inum;
        }


        // directory will update its entries and links 
        rv = directory_delete(get_inode(cow_dir), file);
    }

    free(path_lvar);
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

    char* from_path_lvar = strdup(from);
    char* from_file = basename(from_path_lvar);

    // hard link
    int h_from_inum = directory_lookup(get_inode(s_from_inum), from_file);

    if (h_from_inum < 0)
    {
        return h_from_inum; // should be an error code
    }

    ////////////////////// To inum ///////////////////
    // soft link for directory (are virtually the same thing)
    int s_to_inum = tree_lookup_stop_early(to);
    if (s_to_inum < 0)
    {
        return s_to_inum; // directory does not exist
    }

    char* to_path_lvar = strdup(to);
    char* to_file = basename(to_path_lvar);

    // hard link
    int h_to_inum = directory_lookup(get_inode(s_to_inum), to_file);

    inode* to_dir = get_inode(s_to_inum);
    inode* cow_to_dir = copy_inode(to_dir);

    int rv = 0;

    // the to inum does not exist
    if (h_to_inum < 0)
    {
        rv = directory_put(cow_to_dir, to_file, h_from_inum);

        // update from's references
        inode* from_node = get_inode(h_from_inum);
        ++from_node->refs;
    }
    else
    {
        // to target already exists
        return -EEXIST;
    }

    if(rv < 0) {
        free(from_path_lvar);
        free(to_path_lvar);
        return rv;
    }

    char* path = strdup(from);

    char op[24];
    snprintf(op, sizeof(op), "link %s %s", from, to);
    rv = traverse_and_update(dirname(path), s_to_inum, cow_to_dir->inum, op);

    free(from_path_lvar);
    free(to_path_lvar);
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

    char* path_lvar = strdup(path);
    char* file = basename(path_lvar);

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

    free(path_lvar);
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
    slist* result = NULL;

    int inum = tree_lookup(path);
    if (inum < 0)
    {
        return result;
    }

    slist* children = directory_list(path);
    slist* curr = children;

    inode* base_dir = get_inode(inum);

    while (curr)
    {
        char* curr_name = curr->data;

        // cons the current child to the result
        // printf("base: '%s' curr:'%s'\n", path, curr->data);


        // If thhe current child is a directory, we will
        // need to recur with it
        int curr_inum = directory_lookup(base_dir, curr_name);
        inode* curr_inode = get_inode(curr_inum);


        char* child_path = NULL;
        
        if (!streq(path, "/")) {
            child_path = strdup(path);
            child_path = strcat(child_path, "/");
            child_path = strcat(child_path, curr_name);
        } else {
            child_path = strcat(strdup(path), curr_name);
        }

        if(inode_is_dir(curr_inode)) {
            // printf("\t new base: '%s'\n", child_path);
            result = s_concat(storage_list(child_path), result);
        }

        result = s_cons(++child_path, result);

        curr = curr->next;
    }

    return result;
}
