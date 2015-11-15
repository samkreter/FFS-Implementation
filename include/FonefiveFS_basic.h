#include <sys/types.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <block_store.h>
#include <inttypes.h>
// Probably other things



// Enum to differentiate between different kinds of files
typedef enum {
    REGULAR = 0x01, DIRECTORY = 0x02
} ftype_t;


// They are what they sound like, the max filename (not counting terminator)
// and the number of things a directory can contain
// Have to be exposed in the header for the record structure, which is annoying
// But the other option is to add more functions to parse and handle that struct
#define BLOCK_SIZE 1024
#define FNAME_MAX 47
#define DIR_REC_MAX 20
#define INODE_BLOCK_OFFSET 8
#define INODE_BLOCK_TOTAL 32
#define DATA_BLOCK_MAX 65536
#define DATA_BLOCK_OFFSET (INODE_BLOCK_OFFSET + INODE_BLOCK_TOTAL)
//#define BLOCK_IDX_VALID(block_idx) ((block_idx) >= DATA_BLOCK_OFFSET && (block_idx) < DATA_BLOCK_MAX)
#define DIRECT_TOTAL 6
#define INDIRECT_TOTAL (BLOCK_SIZE / sizeof(block_ptr_t))
#define DBL_INDIRECT_TOTAL (INDIRECT_TOTAL * INDIRECT_TOTAL)
#define FILE_SIZE_MAX ((DIRECT_TOTAL + INDIRECT_TOTAL + DBL_INDIRECT_TOTAL) * BLOCK_SIZE)
#define CURR_BLOCK_INDEX(size) (((size)+1) / BLOCK_SIZE)
#define BLOCKS_NEEDED(size) (((size)+(BLOCK_SIZE-1))/BLOCK_SIZE)
#define OFFSET_IN_BLOCK(size) ((size) % BLOCK_SIZE)
#define BLOCK_IDX_VALID(block_idx) ((block_idx) >= DATA_BLOCK_OFFSET && (block_idx) < DATA_BLOCK_MAX)
#define INCREMENT_VOID_PTR(v_ptr, increment) (((uint8_t *)v_ptr) + (increment))




typedef uint8_t inode_ptr_t;
typedef uint32_t block_ptr_t;

//metadata for the inodes
typedef struct metaData{
    ftype_t filetype;
    //1 means
    char inUse;
    uint32_t size;
	char placeholder[38];
} inode_meta_data_t;

//the individuale inodes
typedef struct iNode {
	char fname[FNAME_MAX+1];
	inode_meta_data_t metaData;
	uint32_t data_ptrs[8];
} iNode_t;

//set up the file system structure
typedef struct F15FS {
	block_store_t *bs;
	iNode_t inodeTable[256];
} F15FS_t;

// It's a directory entry. Won't really be used internally
typedef struct {
	ftype_t ftype;
	char fname[FNAME_MAX+1];
} dir_entry_t;

// It's a directory record, used to report directory contents to the user
// Won't really be used internally
typedef struct dir_rec {
    int total; // total valid entries
    dir_entry_t contents[DIR_REC_MAX];
} dir_rec_t;


//for dir block data/////////////////
//the entry that holds the filename and inode ptr, could be a directory
typedef struct {
    char filename[FNAME_MAX+1];
    ftype_t ftype;
    inode_ptr_t inode;
}dir_block_entry_t;

//metadata for the directory itsself
typedef struct {
    uint32_t size;
    char filler[36];
} dir_meta_t;
//the actuall entry put onto the block itself
typedef struct {
    dir_meta_t metaData;
    dir_block_entry_t entries[DIR_REC_MAX];
} dir_block_t;
//////////////////////////////////////


typedef struct {
    uint8_t found;
    inode_ptr_t inode;
    inode_ptr_t parentDir;
}search_dir_t;

typedef struct{
    block_ptr_t direct_ptr[32];
}indirect_block_t;
///
/// Creates a new F15FS file at the given location
/// \param fname The file to create (or overwrite)
/// \return Error code, 0 for success, < 0 for error
///
int fs_format(const char *const fname);

///
/// sets up a block as a new dir
/// \param bs The blockstore to set it up on
/// \return the pos of the new dir, <0 for erros
///
block_ptr_t setUpDirBlock(block_store_t* bs);

///
/// allocates room for the inodetable in the bs passed in
/// \param bs The block store to be allocated
/// \return <=0 for errors otherwiase success
///
int allocateInodeTableInBS(block_store_t* bs);

///
/// pulls the inode table from bs into memory
/// \param fs the filesystem to pull the inodetable into
/// \return >0 for success otherwise erros
///
int getiNodeTableFromBS(F15FS_t* fs);

///
/// flushed the inodetable to the blocksotre of the fs
/// \param fs the filesytsme to flush the inodetable from
/// \return >0 for success otherwise erros
///
int flushiNodeTableToBS(F15FS_t* fs);



int findEmptyInode(F15FS_t *const fs);
int searchDir(F15FS_t *const fs, char* fname, block_ptr_t blockNum, inode_ptr_t* inodeIndex);
int freeFilePath(char*** pathList);
int getInodeFromPath(F15FS_t *const fs, char** pathList, search_dir_t* searchOutParams);
int parseFilePath(const char *const filePath, char*** pathListOutput);
int addFIleToDir(F15FS_t *const fs, const char *const fname, inode_ptr_t fileInode, inode_ptr_t dirInode, ftype_t ftype);
block_ptr_t writeIndirectBlock(F15FS_t *const fs,size_t *dataLeftTOWrite,const void *data,size_t nbytes,size_t needToAllocate,size_t blocksUsed,block_ptr_t indirectBlockId);
block_ptr_t writeDirectBLock(F15FS_t *const fs,size_t *dataLeftTOWrite,const void *data, size_t offset,size_t nbytes,size_t *needToAllocate, block_ptr_t blockId);
///
/// Mounts the specified file and returns an F15FS object
/// \param fname the file to load
/// \return An F15FS object ready to use, NULL on error
///
F15FS_t *fs_mount(const char *const fname);

/// Unmounts, closes, and destructs the given object,
///  saving all unwritten contents (if any) to file
/// \param fs The F15FS file
/// \return 0 on success, < 0 on error
///
int fs_unmount(F15FS_t *fs);

///
/// Creates a new file in the given F15FS object
/// \param fs the F15FS file
/// \param fname the file to create
/// \param ftype the type of file to create
/// \return 0 on success, < 0 on error
///
int fs_create_file(F15FS_t *const fs, const char *const fname, const ftype_t ftype);

///
/// Returns the contents of a directory
/// \param fs the F15FS file
/// \param fname the file to query
/// \param records the record object to fill
/// \return 0 on success, < 0 on error
///
int fs_get_dir(F15FS_t *const fs, const char *const fname, dir_rec_t *const records);

///
/// Writes nbytes from the given buffer to the specified file and offset
/// Increments the read/write position of the descriptor by the ammount written
/// \param fs the F15FS file
/// \param fname the name of the file
/// \param data the buffer to read from
/// \param nbyte the number of bytes to write
/// \param offset the offset in the file to begin writing to
/// \return ammount written, < 0 on error
///
ssize_t fs_write_file(F15FS_t *const fs, const char *const fname, const void *data, size_t nbyte, size_t offset);

///
/// Reads nbytes from the specified file and offset to the given data pointer
/// Increments the read/write position of the descriptor by the ammount read
/// \param fs the F15FS file
/// \param fname the name of the file to read from
/// \param data the buffer to write to
/// \param nbyte the number of bytes to read
/// \param offset the offset in the file to begin reading from
/// \return ammount read, < 0 on error
///
ssize_t fs_read_file(F15FS_t *const fs, const char *const fname, void *data, size_t nbyte, size_t offset);

///
/// Removes a file. (Note: Directories cannot be deleted unless empty)
/// This closes any open descriptors to this file
/// \param fs the F15FS file
/// \param fname the file to remove
/// \return 0 on sucess, < 0 on error
///
int fs_remove_file(F15FS_t *const fs, const char *const fname);

///
/// Moves the file from the source name to the destination name
/// Note: This does not invalidate any file descriptors
/// \param fs the F15FS file
/// \param fname_src the file to move
/// \param fname_dst the file's new location
/// \return 0 on success, < 0 on error
///
int fs_move_file(F15FS_t *const fs, const char *const fname_src, const char *const fname_dst);
