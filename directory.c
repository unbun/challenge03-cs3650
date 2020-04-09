
#define _GNU_SOURCE
#include <string.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

#include "directory.h"
#include "pages.h"
#include "slist.h"
#include "util.h"
#include "inode.h"

#define ENT_SIZE 16

ddirent*
directory_get_entries(inode* dd) {
    return (ddirent*)pages_get_page(dd->ptrs[0]);
}

int
directory_lookup(inode* dd, const char* name)
{
    // get the entries
    ddirent* entries = directory_get_entries(dd);

    // iterate through the entries
    int size = dd->size / sizeof(ddirent);
    for (int ii = 0; ii < size; ++ii) {

        //check if this entry is what we are looking up
        char* curr_name = entries[ii].name;
        if (streq(curr_name, name)) {
            return entries[ii].inum;
        }
    }
    return -ENOENT;
}


int
tree_lookup(const char* path)
{

    if (streq(path, "/")) {
        return 0;
    }

    path++; //skip the root for s_split
    slist* curr_level = s_split(path, '/'); // get an iterable list of the levels in the path

    int curr_inum = 0;
    while(1) {
        // update the current inum to be the next directory in the path
        inode* dirnode = get_inode(curr_inum);
        curr_inum = directory_lookup(dirnode, curr_level->data);

        // if the next directory in the path is nothing, we finished traversing the path
        if(curr_level->next == 0) {
            return curr_inum;
        }

        // continue traversing the path
        curr_level = curr_level->next;
    }

    return -ENONET;
}

int
directory_put(inode* dd, const char* name, int inum)
{
    // inode should have enough block references
    grow_inode(dd, dd->size + sizeof(ddirent));
    get_inode(inum)->refs++;

    // put get the directory's next empty slot
    ddirent* to_put = (ddirent*)(pages_get_page(dd->ptrs[0]) + dd->size) - 1;

    // populate the spot
    strcpy(to_put->name, name);
    to_put->inum = inum;

    printf("+ dirent_%d = '%s'\n", to_put->inum, to_put->name);

    return 0;
}

int
directory_delete(inode* dd, const char* name)
{
    // Get the entries
    ddirent* entries = directory_get_entries(dd);

    // Iterate through the entries
    int size = dd->size / sizeof(ddirent);
    for (int ii = 0; ii < size; ii += 1)
    {
        // the entries of the dd don't have to be deleted, but their inodes need to
        // to be updated to not be a reference
        if (streq(entries[ii].name, name))
        {
            // decr ref count
            inode* node = get_inode(entries[ii].inum);
            node->refs--;

            // dont need the node if there's no more references
            if (node->refs <= 0)
            {
                free_inode(node);
            }

            // update the entries
            memcpy(&(entries[ii]), &(entries[size - 1]),
                   sizeof(ddirent));
            shrink_inode(dd, dd->size - sizeof(ddirent));
            return 0;
        }
    }

    return -ENOENT;
}

slist*
directory_list(const char* path)
{
    // Get the entries based on the path
    int dirnum = tree_lookup(path);
    inode* dd = get_inode(dirnum);

    ddirent* entries = directory_get_entries(dd);

    // iterate through the entries
    int size = dd->size / sizeof(ddirent);
    slist* names = NULL;
    for (int ii = 0; ii < size; ii += 1)
    {
        // add this entry to the list of names
        char* first = entries[ii].name;
        names = s_cons(first, names);
    }

    return names;
}

void
print_directory(inode* dd)
{
    //  Get the entries
    ddirent* entries = directory_get_entries(dd);

    // iterate through the entries
    int size = dd->size / sizeof(ddirent);
    for (int ii = 0; ii < size; ii += 1)
    {
        // print the entry
        printf("> %s\n", entries[ii].name);
    }
}
