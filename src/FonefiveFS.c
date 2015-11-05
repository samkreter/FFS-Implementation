#include "../include/FonefiveFS_basic.h"




int fs_format(const char *const fname){
	
	if(fname && fname != NULL && strcmp(fname,"") != 0){
		printf("GOt into fs_format\n");
		block_store_t* bs = block_store_create();
		if(bs){
			printf("Block Store Created good\n");
			if(allocateInodeTableInBS(bs) >= 0){
				printf("got through allocation\n");
                //write the block store to the file
                block_store_link(bs, fname);
                printf("got linked up\n");
                return 0;
            }

		}
	}
	return -1;
}


block_ptr_t setUpDirBlock(block_store_t* bs){

    dir_block_t newDir;
    newDir.metaData.size = 0;
    block_ptr_t dirBlockPos;
    printf("In setupdirblock\n");
    if((dirBlockPos = block_store_allocate(bs))){
    	printf("alloction worked in sudb\n");
    	if(block_store_write(bs, dirBlockPos, &newDir, 1024, 0) == 1024){
    		printf("all good in the sudb with \n");
    		return dirBlockPos;
    	}
     
    }
    return -1;
}

int allocateInodeTableInBS(block_store_t* bs){

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
		rootNode.fname[0] = '/';

        //set the file type to a directory
        rootNode.metaData.filetype = DIRECTORY;

		//allocate the root directory node and check it worked
		if((rootNode.data_ptrs[0] = setUpDirBlock(bs)) < 0){
            return -1;
        }

        //write root inode to the blockstore
        if(block_store_write(bs, 8, &rootNode, 48, 0) == 48){
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


int getiNodeTableFromBS(F15FS_t* fs){
	if(fs){
		size_t i = 8;
		uint8_t buffer[1024];
		int startingPos = 0;
		for(i = 8; i < 41; i++){
			startingPos = (i-8)*8;
			if(block_store_read(fs->bs,i,&buffer,1024,0) == 1024){
				if(memcpy(&(fs->inodeTable[startingPos]),&buffer,1024) == NULL){
					return 0;
				}
			}
			else{
				return 0;
			}
		}
		return 1;
	}
	return 0;
}

///
/// Mounts the specified file and returns an F15FS object
/// \param fname the file to load
/// \return An F15FS object ready to use, NULL on error
///
F15FS_t *fs_mount(const char *const fname){
	if(fname && fname != NULL && strcmp(fname,"") != 0){
		block_store_t* bs;
		F15FS_t* fs = malloc(sizeof(F15FS_t));
		if(fs){
			if((bs = block_store_import(fname)) != NULL){
				fs->bs = bs;
				if(getiNodeTableFromBS(fs)){
					return fs;
				}
			}
		}
	}
	return NULL;
}



/// Unmounts, closes, and destructs the given object,
///  saving all unwritten contents (if any) to file
/// \param fs The F15FS file
/// \return 0 on success, < 0 on error
///
int fs_unmount(F15FS_t *fs){
	return 0;
}