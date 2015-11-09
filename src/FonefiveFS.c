#include "../include/FonefiveFS.h"
#include <string.h>
#include <limits.h>

// redefine because it doesn't hurt and it's good to have for refrence
#define FNAME_MAX 47
#define DIR_REC_MAX 20
/*
    typedef enum {
        REGULAR = 0x01, DIRECTORY = 0x02
    } ftype_t;

    // It's a directory entry. Won't really be used internally
    typedef {
        ftype_t ftype;
        char fname[FNAME_MAX+1];
    } dir_entry_t;

    // It's a directory record, used to report directory contents to the user
    // Won't really be used internally
    typedef struct dir_rec {
        int total; // total valid entries
        dir_entry_t contents[DIR_REC_MAX];
    } dir_rec_t;
*/

typedef uint32_t block_ptr_t; // 32-bit addressing, woo!

typedef uint8_t inode_ptr_t; // inodes go from 0 - 255

// That's it?
struct F15FS {
    block_store_t *bs;
};

// got 48 bytes to spare
typedef struct {
    uint32_t size; // Meaningless if it's a directory
    uint32_t mode; // Not actually used
    uint32_t c_time; // technically c_time is for modifications to mode and stuff... we'll use it as creation
    uint32_t a_time;
    uint32_t m_time;
    inode_ptr_t parent; // handy!
    uint8_t type; // haha, whoops, almost forgot it!
    // I'd prefer it was the actual type enum, but the size of that is... hard to pin down
    // Uhhh... 26 bytes left...
    uint8_t padding[26];
} mdata_t;

// it's an inode...
typedef struct {
    char fname[FNAME_MAX + 1];
    mdata_t mdata;
    block_ptr_t data_ptrs[8];
} inode_t;


// directory entry, found inside directory blocks
typedef struct {
    char fname[FNAME_MAX + 1];
    inode_ptr_t inode;
} dir_ent_t;

// Sadly, not the same size as an mdata_t, but it's close! 44 Bytes
// They can really be cast back and forth safely since the end is padding anyway
// But if you have an array of them, it could get messy, but why would you ever have one of those?
typedef struct {
    uint32_t size;
    // Number of VALID entries, ENTRIES ARE NOT CONTIGUOUS
    // Which means all entries will need to be scanned every time
    // Maintaining contiguity can be done, and in a transaction safe way, but it's extra work

    // All metadata is contained in the inode, except for size
    // Which leaves... 40B
    uint8_t padding[40];
} dir_mdata_t;

// A directory file. Directories only have one for simplicity
typedef struct {
    dir_mdata_t mdata;
    dir_ent_t entries[DIR_REC_MAX];
} dir_block_t;


// size of data block
#define BLOCK_SIZE 1024

typedef uint8_t data_block_t[BLOCK_SIZE]; // data's just data, also c is weird

// total inode blocks
#define INODE_BLOCK_TOTAL 32

// Number of inodes in a block
#define INODES_PER_BLOCK (BLOCK_SIZE / sizeof(inode_t))

// Total number of inodes
#define INODE_TOTAL ((INODE_BLOCK_TOTAL * BLOCK_SIZE) / sizeof(inode_t))

// Inode blocks start at 8 and go to 40
#define INODE_BLOCK_OFFSET 8

#define DATA_BLOCK_OFFSET (INODE_BLOCK_OFFSET + INODE_BLOCK_TOTAL)

// 6 direct blocks per inode
#define DIRECT_TOTAL 6
// number of directs in an indirect, same as in an inode
#define INDIRECT_TOTAL (BLOCK_SIZE / sizeof(block_ptr_t))
// number of directs in a full double indirect, same as in an inode
#define DBL_INDIRECT_TOTAL (INDIRECT_TOTAL * INDIRECT_TOTAL)

// I did the math... but it can also be calculated
#define FILE_SIZE_MAX ((DIRECT_TOTAL + INDIRECT_TOTAL + DBL_INDIRECT_TOTAL) * BLOCK_SIZE)

// Total available data blocks since the block_store is smaller than the indexing allows
// Might not be used
#define DATA_BLOCK_MAX 65536

// It should really be less (about 12100)
// But we'll overallocate, preferably not on the stack...
#define FS_PATH_MAX 13000

// Calcs what block an inode is in
#define INODE_TO_BLOCK(inode) (((inode) >> 3) + INODE_BLOCK_OFFSET)

// Calcs the index an inode is at within a block
#define INODE_INNER_IDX(inode) ((inode) & 0x07)

// Calcs the offset of an inode because I made block_store too kind
// and I can't undo it. Immagine how much of a pain it'd be if you could only read/write entire blocks
// (...like how actual block devices work)
#define INODE_INNER_OFFSET(inode) (INODE_INNER_IDX(inode) * sizeof(inode_t))

// Converts a file position to a block index (note: not a block id. index 6 is the 6th block of the file)
#define POSITION_TO_BLOCK_INDEX(position) ((position) >> 10)

// Position within a block
#define POSITION_TO_INNER_OFFSET(position) ((position) & 0x3FF)

// tells you if the given block index makes sense
#define BLOCK_IDX_VALID(block_idx) ((block_idx) >= DATA_BLOCK_OFFSET && (block_idx) < DATA_BLOCK_MAX)

// Checks that an inode is the specified type
// Used because the mdata type won't be the same size as the enum and that'll irritate the compiler probably
#define INODE_IS_TYPE(inode_ptr, file_type) ((inode_ptr)->mdata.type & (file_type))

// Because you can't increment an incomplete type
// And writing out the cast every time gets tiring
#define INCREMENT_VOID_PTR(v_ptr, increment) (((uint8_t *)v_ptr) + (increment))


// Casual warning, if it hasn't been used yet, it might not work yet.
// Also, my bugs are your bugs if you decide to use this. be careful.

//
// Creates a new F15FS file at the given location
// param fname The file to create (or overwrite)
// return Error code, 0 for success, < 0 for error
//
// This is why gotos for resource unwinding is nice. Look at this mess.
int fs_format(const char *const fname) {
    // Gotta do a create and then a link
    if (fname && fname[0]) { // Well, it works.
        block_store_t *bs = block_store_create();
        if (bs) {
            block_store_link(bs, fname);
            if (block_store_errno() == BS_OK) {
                // Ok, made a block store, linked and made the file, now to fill out the inodes
                //  and by that I mean, request the inode table and a data block
                //  and fill out root and its data block
                bool success = true;
                for (size_t i = INODE_BLOCK_OFFSET; i < (INODE_BLOCK_OFFSET + 32); ++i) {
                    success &= block_store_request(bs, i); // haha! C standard says that true is 1, so this works
                }
                if (success) {
                    size_t root_file_block = block_store_allocate(bs); // don't really care where it goes
                    if (root_file_block) {
                        // Ok, NOW we're good... assuming all the writes work. Ugh.
                        inode_t root;
                        dir_block_t root_dir;
                        memset(&root, 0, sizeof(inode_t));
                        memset(&root_dir, 0, sizeof(dir_block_t));

                        root.fname[0] = '/'; // Technically probably an invalid name, but less invalid than an empty string
                        root.mdata.size = 0; // Uhhh...? Not really going to use it in a directory
                        root.mdata.mode = 0; // it's not being used, who cares.
                        root.mdata.c_time = time(NULL); // Time in seconds since epoch. 32-bit time is a problem for 2038
                        root.mdata.a_time = time(NULL); // I guess technically never?
                        root.mdata.m_time = time(NULL); // technically later, but ehhh.
                        root.mdata.parent = 0; // Us!
                        root.mdata.type = (uint8_t) DIRECTORY;
                        root.data_ptrs[0] = root_file_block; // the rest have init to 0

                        root_dir.mdata.size = 0; // Uhhh...?

                        if (block_store_write(bs, INODE_TO_BLOCK(0), &root, sizeof(inode_t), 0) == sizeof(inode_t) &&
                                block_store_write(bs, root_file_block, &root_dir, sizeof(dir_block_t), 0) == sizeof(dir_block_t)) {
                            // technically we could loop till it goes, but the way it's written, if it didn't work the first time,
                            //  something's super broke

                            // Ok... Done? Req'd the table, setup root and its folder, wrote it all, time to sync.
                            block_store_destroy(bs, BS_FLUSH);
                            // flushing sets the block_store_errno() to the flush status, so...
                            if (block_store_errno() == BS_OK) {
                                // FINALLY.
                                return 0;
                            }
                            fprintf(stderr, "Flush died, block_store says: %s\n", block_store_strerror(block_store_errno()));
                            // can't destruct because we already did
                            return -1;
                        }
                        fprintf(stderr, "Something didn't write, block_store says: %s\n", block_store_strerror(block_store_errno()));
                        block_store_destroy(bs, BS_FLUSH); // Flushing might not work, but if it does, it might have nice debug info
                        return -1;
                    }
                    fprintf(stderr, "Couldn't request root file, block_store says: %s\n", block_store_strerror(block_store_errno()));
                    block_store_destroy(bs, BS_FLUSH);
                    return -1;
                }
                fprintf(stderr, "Couldn't request inode table, block_store says: %s\n", block_store_strerror(block_store_errno()));
                block_store_destroy(bs, BS_FLUSH);
                return -1;
            }
            fprintf(stderr, "Couldn't link block_store, block_store says: %s\n", block_store_strerror(block_store_errno()));
            block_store_destroy(bs, BS_NO_FLUSH);
            return -1;
        }
        fprintf(stderr, "Couldn't create block_store, block_store says: %s\n", block_store_strerror(block_store_errno()));
        return -1;
    }
    fprintf(stderr, "Filename \"%s\" invalid\n", fname); // It'll either print "(null)" or ""
    return -1;
}

//
// Mounts the specified file and returns an F15FS object
// param fname the file to load
// return An F15FS object ready to use, NULL on error
//
F15FS_t *fs_mount(const char *const fname) {
    // open the bs object and... that's it?
    F15FS_t *fs = malloc(sizeof(F15FS_t));
    if (fs) {
        // this will catch all fname issues
        fs->bs = block_store_import(fname);
        if (fs->bs) {
            if (block_store_errno() == BS_OK) {
                return fs;
            }
            fprintf(stderr, "Issue with import (link problem?), block_store says: %s", block_store_strerror(block_store_errno()));
            block_store_destroy(fs->bs, BS_NO_FLUSH);
            free(fs);
            return NULL;
        }
        fprintf(stderr, "Couldn't open file \"%s\", block_store says: %s\n", fname, block_store_strerror(block_store_errno()));
        free(fs);
        return NULL;
    }
    fprintf(stderr, "Couldn't malloc fs object");
    return NULL;
}

// Unmounts, closes, and destructs the given object,
//  saving all unwritten contents (if any) to file
// param fs The F15FS file
// return 0 on success, < 0 on error. Object is always destructed, if it exists
//
int fs_unmount(F15FS_t *fs) {
    if (fs) {
        block_store_destroy(fs->bs, BS_FLUSH);
        free(fs);
        if (block_store_errno() == BS_OK) {
            return 0;
        }
        fprintf(stderr, "BS_DESTROY failed to flush? Block_store says: %s", block_store_strerror(block_store_errno()));
    }
    return -1;
}
