#include "../include/FonefiveFS_basic.h"




int fs_format(const char *const fname){
	if(fname){
		block_store_t* bs = block_store_create();
		if(bs){
			if(allocateInodeTableInBS(bs) >= 0){
                //write the block store to the file
                block_store_link(bs, fname);
                return 1;
            }

		}
	}
	return -1;
}


dir_ptr_t setUpDirBlock(const blockstore_t* bs){

    dir_block_t newDir;
    newDir.meteData.size = 0;
    dir_ptr_t dirBlockPos;

    if(dirBlockPos = block_store_allocate(bs)){
        if(write_to_back_store(newDir,dirBlockPos,1024)){
            return dirBlockPos;
        }
    }
    return -1;
}

int allocateInodeTableInBS(const blockstore_t* bs){

    if(bs){
		size_t i = 0;
        //allocate the room for the inode table
		for(i=8; i<41; i++){
			if(!block_store_request(bs,i)){
				return -1;
			}
		}

		iNode_t rootNode;
        //set root filename as '/'
		rootNode.fname = "/";

        //set the file type to a directory
        rootNode.meteData.filetype = DIRECTORY;

		//allocate the root directory node and check it worked
		if((rootNode.data_ptrs[0] = setUpDirBlock(bs)) < 0){
            return -1;
        }

        //write root inode to the blockstore
		if(write_to_back_store(rootNode,8,48)){
			return 1;
		}
		return -11;

	}
	return -1;
}

// iNode_t* createInodeTable(block_store_t* bs){
// 	iNode_t* iNodeTable = malloc(sizeof(iNodeTable_t)*256);

// 	if(iNodeTable){
// 		iNodeTable[0].fname = "/";
// 		iNodeTable[0].data_ptrs[0] = 8;
// 		return iNodeTable;

// 		// for(i = 8; i < 41; i++){
// 		// 	if(block_store_request(bs,i)){
// 		// 		iNode_t buffer[8];
// 		// 		int startingPos = (i-8)*8;
// 		// 		memcpy(buffer,iNodeTable[startingPos],sizeof(iNode_t)*8);
// 		// 		if(!write_to_back_store(buffer,i)){
// 		// 			printf("Failed to write to backing store for inode Creatation")
// 		// 		}
// 		// 	}else{
// 		// 		printf("Could not allocate the needed inodeTable\n");
// 		// 		return NULL;
// 		// 	}
// 		// }


// 	}
// 	return NULL;
// }

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