#include "../include/FonefiveFS_basic.h"

#define BLOCK_SIZE 1024
#define ROOT_INODE 0



int fs_format(const char *const fname){
	//param check
	if(fname && fname != NULL && strcmp(fname,"") != 0){
		//create the blockstore
		block_store_t* bs = block_store_create();
		if(bs){
			//use util funt o allocate all the spaces needed in it.
			//really just to make sure we set the front blocks to used
			//in the bitmap for the inode table so they can't be allocated as data blcoks
			if(allocateInodeTableInBS(bs) >= 0){

                //write the block store to the file
                block_store_link(bs, fname);

                return 0;
            }

		}
	}
	return -1;
}


block_ptr_t setUpDirBlock(block_store_t* bs){
	//param check
	if(bs){
		//declare the new dir
	    dir_block_t newDir;

	    //set the size of its contents to zero
	    newDir.metaData.size = 0;

	    //declare it pos
	    block_ptr_t dirBlockPos;

	    //get the next free block in the blockstore
	    if((dirBlockPos = block_store_allocate(bs))){
	    	//write the data of the dir to the block and get teh pos to put in the inode table
	    	if(block_store_write(bs, dirBlockPos, &newDir, BLOCK_SIZE, 0) == BLOCK_SIZE){
	    		return dirBlockPos;
	    	}

	    }
	}
    return -1;
}

int allocateInodeTableInBS(block_store_t* bs){
	//param check,
    if(bs){

		//declare i because c just isn't as cool as c++ with declaring in the loop
		size_t i = 0;
        //allocate the room for the inode table
		for(i=8; i<41; i++){
			if(!block_store_request(bs,i)){
				return -1;
			}
		}

		//create a root inode
		iNode_t rootNode;

        //set root filename as '/'
		rootNode.fname[0] = '/';

        //set the file type to a directory
        rootNode.metaData.filetype = DIRECTORY;
        rootNode.metaData.inUse = 1;

		//allocate the root directory node and check it worked
		if((rootNode.data_ptrs[0] = setUpDirBlock(bs)) < 0){
            return -1;
        }

        //write root inode to the blockstore
        if(block_store_write(bs, 8, &rootNode, 128, 0) == 128){
			return 1;
		}
		return -11;

	}
	return -1;
}


int flushiNodeTableToBS(F15FS_t* fs){
	//I'm really lovin all these param checks
	if(fs && fs->bs != NULL){
		//I think size_t is some cool stuff, you know causes it looks special
		size_t i = 8;
		//another kilobyte of fun
		char buffer[BLOCK_SIZE];
		//index mapping var to go from i to the correct index of the inodetable
		int startingPos = 0;
		for(i = 8; i < 41; i++){
			//increment the mapping var
			startingPos = (i-8)*8;
			//take the memory to a buffer before we write to the table in casue of
			// some funny bussiness in the blockstore
			if(memcpy(&buffer,(fs->inodeTable+startingPos),BLOCK_SIZE) != NULL){
				//write our stuff to the block store
				if(block_store_write(fs->bs,i,&buffer,BLOCK_SIZE,0) != BLOCK_SIZE){
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

int getiNodeTableFromBS(F15FS_t* fs){
	//usual param checks
	if(fs){
		//starting at the 8th block in the blockstore
		size_t i = 8;
		//set up the buffer to be 1 kilobyte
		uint8_t buffer[BLOCK_SIZE];
		//create a maping var to get the index of the inodetable
		int startingPos = 0;
		for(i = 8; i < 40; i++){
			//increment the index mapper
			startingPos = (i-8)*8;
			//reak a block from the bs
			if(block_store_read(fs->bs,i,&buffer,BLOCK_SIZE,0) == BLOCK_SIZE){
				//put the contents into the fs inode table
				if(memcpy((fs->inodeTable+startingPos),&buffer,BLOCK_SIZE) == NULL){
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


F15FS_t *fs_mount(const char *const fname){
	//check params, having to check for weird things
	if(fname && strcmp(fname,"") != 0){
		//create the filesytem struct
		F15FS_t* fs = (F15FS_t*)malloc(sizeof(F15FS_t));
		if(fs){
			//import the blockstore from the file
			if((fs->bs = block_store_import(fname)) != NULL){
				//pull the inode table into memory
				if(getiNodeTableFromBS(fs)){
					return fs;
				}
			}
		}
	}
	return NULL;
}



int fs_unmount(F15FS_t *fs){
	//check params
	if(fs && fs->inodeTable && fs->bs){
		//flush the inode table into the blockstore
		if(flushiNodeTableToBS(fs)){
			//flush the blockstore to the file
			block_store_flush(fs->bs);
			if(block_store_errno() == BS_OK){
				//free the blockstore
				block_store_destroy(fs->bs,BS_NO_FLUSH);
				//free the file system pointer
				free(fs);
				return 0;
			}

		}
	}
	return -1;
}



int findEmptyInode(F15FS_t *const fs){
    if(fs){
        int i = 0;
        for(i = 0; i < 256; i++){
            if(((int)fs->inodeTable[i].metaData.inUse) != 1){
                return i;
            }
        }
    }
    return -1;
}

//0 for not there -1 for actual error 1 for found
int searchDir(F15FS_t *const fs, char* fname, block_ptr_t blockNum, inode_ptr_t* inodeIndex){
    dir_block_t dir;
    if(block_store_read(fs->bs,blockNum,&dir,BLOCK_SIZE,0) == BLOCK_SIZE){
        int i = 0;
        printf("dir size = %d\n",dir.metaData.size);
        for(i = 0; i < dir.metaData.size; i++){
            printf("insed SD name: %s\n",dir.entries[i].filename);
            if(strcmp(dir.entries[i].filename,fname) == 0){
                *inodeIndex = dir.entries[i].inode;
                return 1;
            }
        }
        return 0;
    }
    return -1;
}

int freeFilePath(char*** pathList){
    int listSize = (int)*(*pathList)[0];
    int i = 0;
    for(i = 0; i < listSize + 1; i++){
        free((*pathList)[i]);
        (*pathList)[i] = NULL;
    }
    free(*pathList);
    *pathList = NULL;
    return 1;
}


// 1 found 0 not found but still filled <0 error
int getInodeFromPath(F15FS_t *const fs, char** pathList, search_dir_t* searchOutParams){
    if(fs && pathList && searchOutParams){
        int listSize = (int)*pathList[0];
        int i = 1;
        //start it at the root node
        inode_ptr_t currInode = ROOT_INODE;
        inode_ptr_t parentInode = ROOT_INODE;
        int result = 0;

        for(i = 1; i < listSize + 1; i++){
            result = searchDir(fs, pathList[i], fs->inodeTable[parentInode].data_ptrs[0], &currInode);
            printf("Result is: %d\n",result);
            printf("looking for %s\n",pathList[i]);
            printf("list size %d, i = %d\n",listSize,i);
            if(result == 0){
                //not found but at the end of list, populate with parent direct
                //for creating a file in that spot
                if((listSize - i) == 0){
                    searchOutParams->found = 0;
                    searchOutParams->parentDir = parentInode;
                    printf("empty and not found\n");
                    return 0;
                }
                else{
                    //bad path
                    searchOutParams->found = 0;
                    printf("bad path1\n");
                    return -1;
                }


            }
            else if(result == 1){
                if((listSize - i) != 0 && fs->inodeTable[currInode].metaData.filetype == DIRECTORY){
                    parentInode = currInode;
                    printf("continuing\n");
                    continue;
                }
                else if((listSize - i) == 0 && fs->inodeTable[currInode].metaData.filetype == DIRECTORY){
                    //can't end it path with directory
                    printf("can't end with directory1\n");
                    return -2;
                }
                else if((listSize - i) != 0 && fs->inodeTable[currInode].metaData.filetype == REGULAR){
                    //can't have file not at end of path
                    printf("can't have file not at end of file\n");
                    return -3;
                }
                else if((listSize - i) == 0 && fs->inodeTable[currInode].metaData.filetype == REGULAR){
                    searchOutParams->found = 1;
                    searchOutParams->parentDir = parentInode;
                    searchOutParams->inode = currInode;
                    printf("all good found file\n");
                    return 1;
                }
            }


         }
         printf("nobdy likes you\n");
         return 1;

    }
    //bad params
    printf("bad parasm2\n");
    return -1;
}

int parseFilePath(const char *const filePath, char*** pathListOutput){
    if(filePath && strcmp(filePath,"") != 0){
        char* nonConstFilePath = malloc(strlen(filePath)+1);
        if(!nonConstFilePath){
            return -1;
        }
        strcpy(nonConstFilePath,filePath);
        char* temp = nonConstFilePath;
        char** pathList = NULL;
        int count = 0;
        const char *delim = "/";
        char* token;

        while(*temp){
            if(*temp == '/'){
                count++;
            }
            temp++;
        }

        if(count == 0){
            //theres only a file or directory name so just add the count
            //and whats in there to one
            if((pathList = (char**)malloc(sizeof(char*)*2)) != NULL){
                if((pathList[0] = (char*)malloc(sizeof(char))) != NULL){
                    *pathList[0] = 1;
                    pathList[1] = nonConstFilePath;
                    *pathListOutput = pathList;
                    return 1;
                }
                free(pathList);
            }
            free(nonConstFilePath);
            fprintf(stderr, "error during mallocing\n");
            return -1;
        }else{
            //create string array with right size plus one to add the size in
            if((pathList = (char**)malloc(sizeof(char*)*(count+1))) == NULL){
                free(nonConstFilePath);
                fprintf(stderr, "error during mallocing\n");
                return -1;
            }

            if((pathList[0] = malloc(sizeof(char))) == NULL){
                free(nonConstFilePath);
                free(pathList);
                fprintf(stderr, "error during mallocing\n");
                return -1;
            }

            //put the length at the begging
            *pathList[0] = count;

            token = strtok(nonConstFilePath,delim);

            int i = 1;
            while(token != NULL){
                if(strlen(token) > FNAME_MAX){
                    for(i = i - 1;i >= 0; i--){
                        free(pathList[i]);
                    }
                    free(pathList);
                    free(nonConstFilePath);
                    fprintf(stderr,"File or Dir name to long\n");
                    return -1;
                }
                if((pathList[i] = (char*)malloc(strlen(token) + 1)) == NULL){
                    for(;i >= 0; i--){
                        free(pathList[i]);
                    }
                    free(pathList);
                    free(nonConstFilePath);
                    fprintf(stderr, "Error during mallocing\n");
                    return -1;
                }

                strcpy(pathList[i],token);

                token = strtok(NULL,delim);
                i++;
            }
            free(nonConstFilePath);
            *pathListOutput = pathList;
            return 1;

        }
    }
    fprintf(stderr, "bad params\n");
    return -1;
}

int addFIleToDir(F15FS_t *const fs, const char *const fname, inode_ptr_t fileInode, inode_ptr_t dirInode, ftype_t ftype){
    if(fs && fname && strcmp(fname,"") != 0 && dirInode >= 0){
        dir_block_t dir;
        if(block_store_read(fs->bs,fs->inodeTable[dirInode].data_ptrs[0],&dir,BLOCK_SIZE,0) == BLOCK_SIZE){
            if(dir.metaData.size < DIR_REC_MAX && strlen(fname) <= FNAME_MAX){
                dir.entries[dir.metaData.size+1].inode = fileInode;
                dir.entries[dir.metaData.size+1].ftype = ftype;
                strcpy(dir.entries[dir.metaData.size].filename, fname);
                printf("file  name p::: %s\n",dir.entries[dir.metaData.size].filename);
                dir.metaData.size++;
                //write the block back to the store
                printf("size %d\n",dir.metaData.size);
                if(block_store_write(fs->bs,fs->inodeTable[dirInode].data_ptrs[0],&dir,BLOCK_SIZE,0) == BLOCK_SIZE){
                    return 1;
                }
                printf("failed to write to block afto din\n");
            }
            printf("couldn't add to dir1\n");
            return -1;
        }
        printf("coulnd't add to dir 2\n");
        return -1;
    }
    printf("bad params addto din\n");
    return -1;
}

///
/// Creates a new file in the given F15FS object
/// \param fs the F15FS file
/// \param fname the file to create
/// \param ftype the type of file to create
/// \return 0 on success, < 0 on error
///
int fs_create_file(F15FS_t *const fs, const char *const fname, const ftype_t ftype){
    //param check
    if(fs && fname && strcmp(fname,"") != 0 && strcmp(fname,"/") != 0&& ftype){
        int emptyiNodeIndex = findEmptyInode(fs);
        char **pathList = NULL;
        int listSize = 0;
        search_dir_t dirInfo;
        if(parseFilePath(fname,&pathList) > 0){
            if(getInodeFromPath(fs,pathList, &dirInfo) == 0){
                listSize = (int)*pathList[0];
                //set the use byte
                fs->inodeTable[emptyiNodeIndex].metaData.inUse = 1;
                fs->inodeTable[emptyiNodeIndex].metaData.filetype = ftype;

                //add the fname to the inode
                strcpy(fs->inodeTable[emptyiNodeIndex].fname,pathList[listSize]);
                addFIleToDir(fs, pathList[listSize], emptyiNodeIndex,dirInfo.parentDir, ftype);

                if(ftype == DIRECTORY){
                    if((fs->inodeTable[emptyiNodeIndex].data_ptrs[0] = setUpDirBlock(fs->bs)) > 0){
                        return 0;
                    }
                }else{
                    //no need to do anything if its just a file, need to actual have things be writen
                    return 0;
                }
            }
        }
    }
    return -1;
}

///
/// Returns the contents of a directory
/// \param fs the F15FS file
/// \param fname the file to query
/// \param records the record object to fill
/// \return 0 on success, < 0 on error
///
int fs_get_dir(F15FS_t *const fs, const char *const fname, dir_rec_t *const records){
    if(fs && fname && fname[0] && records){
        char** pathList = NULL;
        search_dir_t dirInfo;
        if(parseFilePath(fname, &pathList) > 0){
            if(getInodeFromPath(fs,pathList,&dirInfo) > 0){
                dir_block_t dir;
                if(block_store_read(fs->bs,fs->inodeTable[dirInfo.inode].data_ptrs[0],&dir,BLOCK_SIZE,0) == BLOCK_SIZE){
                    int i = 0;
                    records->total = dir.metaData.size;
                    for(i = 0; i < dir.metaData.size; i++){
                        strcpy(records->contents[i].fname, dir.entries[i].filename);
                        records->contents[i].ftype = dir.entries[i].ftype;
                    }
                    return 0;
                }
            }
        }

    }
    return -1;
}

block_ptr_t writeDirectBLock(fs,&dataLeftTOWrite,data,offset,nbytes,needToAllocate, blockId){
    if(fs && *dataLeftTOWrite > 0 && data && nbytes > 0){
        size_t dataOffset = nbytes - *dataLeftTOWrite;

        if(BLOCK_IDX_VALID(blockId) && needToAllocate > 0){

        }
        else{

        }


    }
    //return greatest value for errors
    return (uint32_t)-1;
}

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
ssize_t fs_write_file(F15FS_t *const fs, const char *const fname, const void *data, size_t nbyte, size_t offset){
    if(fs && fname && fname[0] && data && nbytes > 0 && (offset + nbyte) < FILE_SIZE_MAX){
        char **pathList = NULL;
        int listSize = 0;
        size_t currFileSize = 0;
        size_t blocksUsed = 0;
        size_t needToAllocate = 0;
        //if something goes wrong it wont try and write set to zero
        size_t dataLeftTOWrite = 0;
        inode_ptr_t currFileIndex = 0;
        search_dir_t dirInfo;

        if(parseFilePath(fname,&pathList) > 0){

            if(getInodeFromPath(fs,pathList, &dirInfo) > 0){
                //set up the
                listSize = (int)*pathList[0];
                currFileIndex = dirInfo->inode;
                currFileSize = fs->inodeTable[currFileIndex].metaData.size;
                if(offset > currFileSize){
                    fprintf(stderr, "Offset bigger than size, this leaves holes\n", );
                    return -1;
                }
                //check when its time to start adding new blocks instead of writing over them
                needToAllocate = currFileSize - offset;
                blocksUsed = CURR_BLOCK_INDEX(offset);
                dataLeftTOWrite = nbyte;

                while(dataLeftTOWrite != 0){
                    if(blocksUsed >= 0 || blocksUsed <= DIRECT_TOTAL-1){
                        fs->inodeTable[currFileIndex].data_ptrs[blocksUsed] = writeDirectBLock(fs,&dataLeftTOWrite,data,OFFSET_IN_BLOCK(offset),nbytes,needToAllocate,fs->inodeTable[currFileIndex].data_ptrs[blocksUsed]);
                    }
                    else if(blocksUsed >= DIRECT_TOTAL || blocksUsed < INDIRECT_TOTAL){

                    }
                    else if(blocksUsed >= INDIRECT_TOTAL || blocksUsed < DBL_INDIRECT_TOTAL){

                    }
                    else{
                        fprintf(stderr,"file to big after already checked, so pretty weird error\n");
                        return -1;
                    }
                }

            }
        }
    }
    fprintf(stderr, "bad params while writing\n", );
    return -1;
}

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
ssize_t fs_read_file(F15FS_t *const fs, const char *const fname, void *data, size_t nbyte, size_t offset){

}

///
/// Removes a file. (Note: Directories cannot be deleted unless empty)
/// This closes any open descriptors to this file
/// \param fs the F15FS file
/// \param fname the file to remove
/// \return 0 on sucess, < 0 on error
///