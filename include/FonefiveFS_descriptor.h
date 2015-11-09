#ifndef FONEFIVE_H__
#define FONEFIVE_H__

#include <sys/types.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <block_store.h>
// Probably other things

typedef struct F15FS F15FS_t;

// Enum to differentiate between different kinds of files
typedef enum {
    REGULAR = 0x01, DIRECTORY = 0x02
} ftype_t;

// Enum for seeking. They do what they sound like. Check the seek man pages
typedef enum {
	FS_SEEK_BEGIN = 0x01, FS_SEEK_CUR = 0x02, FS_SEEK_END = 0x04
} seek_t;

// They are what they sound like, the max filename (not counting terminator)
// and the number of things a directory can contain
// Have to be exposed in the header for the record structure, which is annoying
// But the other option is to add more functions to parse and handle that struct
#define FNAME_MAX 47
#define DIR_REC_MAX 20

// It's a directory entry. Won't really be used internally
typedef {
	ftype_t ftype;
	char fname[FNAME_MAX+1];
} dir_entry_t;

// It's a directory record, used to report directory contents to the user
// Won't really be used internally
typedef struct dir_rec {
    unsigned total; // total valid entries
    dir_entry_t contents[DIR_REC_MAX];
} dir_rec_t;

///
/// Creates a new F15FS file at the given location
/// \param fname The file to create (or overwrite)
/// \return Error code, 0 for success, < 0 for error
///
int fs_format(const char *const fname);

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
int fs_get_dir(const F15FS_t *const fs, const char *const fname, dir_rec_t *const records);

///
/// Opens a file for read/write (note: Directories cannot be opened)
/// \param fs the F15FS file
/// \param fname the file to open
/// \return File descriptor on success, < 0 on error
///
int fs_open_file(F15FS_t *const fs, const char * const fname);

///
/// Closes an open file
/// \param fs the F15FS file
/// \param fd the file descriptor
/// \return 0 on success, < 0 on error
///
int fs_close_file(F15FS_t *const fs, const int fd);

///
/// Seeks the read/write position of the given descriptor
/// \param fs the F15FS file
/// \param fd the file descriptor
/// \param offset the offset relative to whence
/// \param whence the position from which the offset is based
/// \return 0 on success < 0 on error
///
int fs_seek_file(F15FS_t *const fs, const int fd, const off_t offset, const seek_t whence);

///
/// Writes nbytes from the given buffer to the specified file and offset
/// Increments the read/write position of the descriptor by the ammount written
/// \param fs the F15FS file
/// \param fd the file descriptor
/// \param data the buffer to read from
/// \param nbyte the number of bytes to write
/// \param offset the offset in the file to begin writing to
/// \return ammount written, < 0 on error
///
ssize_t fs_write_file(F15FS_t *const fs, const int fd, const void *data, size_t nbyte, size_t offset);

///
/// Reads nbytes from the specified file and offset to the given data pointer
/// Increments the read/write position of the descriptor by the ammount read
/// \param fs the F15FS file
/// \param fd the file descriptor
/// \param data the buffer to write to
/// \param nbyte the number of bytes to read
/// \param offset the offset in the file to begin reading from
/// \return ammount read, < 0 on error
///
ssize_t fs_read_file(F15FS_t *const fs, const int fd, void *data, size_t nbyte, size_t offset);

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

#endif
