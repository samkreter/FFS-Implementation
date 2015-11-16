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
    //paraam check
    if(fs){
        int i = 0;
        //search through to find first not used inode in table
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
    //param check
    if(fs && fname && fname[0]){
	    dir_block_t dir;
	    // read dir from the blockstore
	    if(block_store_read(fs->bs,blockNum,&dir,BLOCK_SIZE,0) == BLOCK_SIZE){
	        int i = 0;
	        //loop through comparing the filename to see if its in the dir
	        for(i = 0; i < dir.metaData.size; i++){
	            if(i > 20){
	            	return -1;
	            }
	            if(strcmp(dir.entries[i].filename,fname) == 0){
	                //found it, so return 
	                *inodeIndex = dir.entries[i].inode;
	                return 1;
	            }
	        }
	        return 0;
    	}
    	fprintf(stderr,"Failed to read from hd in search directoy\n");
    	exit(-1);
    }
    fprintf(stderr,"Failed param check while searching the directory\n");
    return -1;
}

int freeFilePath(char*** pathList){
    //get the size of the list, i put it at the front
    int listSize = (int)*(*pathList)[0];
    int i = 0;
    // free all the strings 
    for(i = 0; i < listSize + 1; i++){
        free((*pathList)[i]);
        (*pathList)[i] = NULL;
    }
    //free the list itself
    free(*pathList);
    *pathList = NULL;
    return 1;
}


// 1 found 0 not found but still filled <0 error
int getInodeFromPath(F15FS_t *const fs, char** pathList, search_dir_t* searchOutParams){
    if(fs && pathList && searchOutParams){
        //get list size 
        int listSize = (int)*pathList[0];


        int i = 1;
        //start it at the root node
        inode_ptr_t currInode = ROOT_INODE;
        inode_ptr_t parentInode = ROOT_INODE;
        int result = 0;

        //loop through each filename in the list
        for(i = 1; i < listSize + 1; i++){
            result = searchDir(fs, pathList[i], fs->inodeTable[parentInode].data_ptrs[0], &currInode);
            // if the filename was not found
            if(result == 0){
                //not found but at the end of list, populate with parent direct
                //for creating a file in that spot
                if((listSize - i) == 0){
                    searchOutParams->found = 0;
                    searchOutParams->parentDir = parentInode;
                    return 0;
                }
                else{
                    //bad path
                    searchOutParams->found = 0;
                    fprintf(stderr,"bad path 1\n");
                    return -1;
                }


            }
            //if the filename was found 
            else if(result == 1){
            	//keep searching
                if((listSize - i) != 0 && fs->inodeTable[currInode].metaData.filetype == DIRECTORY){
                    parentInode = currInode;
                    continue;
                }
                //we just ended with a dir, works good if i want to read a dir
                else if((listSize - i) == 0 && fs->inodeTable[currInode].metaData.filetype == DIRECTORY){
                    //can't end it path with directory
                    searchOutParams->found = 1;
                    searchOutParams->parentDir = parentInode;
                    searchOutParams->inode = currInode;
                    return -2;
                }
                else if((listSize - i) != 0 && fs->inodeTable[currInode].metaData.filetype == REGULAR){
                    //can't have file not at end of path
                    return -3;
                }
                //found a file and its at the end o the list
                else if((listSize - i) == 0 && fs->inodeTable[currInode].metaData.filetype == REGULAR){
                    searchOutParams->found = 1;
                    searchOutParams->parentDir = parentInode;
                    searchOutParams->inode = currInode;
                    return 1;
                }
            }


         }
         return -1;

    }
    //bad params
    fprintf(stderr,"bad params while getting inode\n");
    return -1;
}

int parseFilePath(const char *const filePath, char*** pathListOutput){
    if(filePath && strcmp(filePath,"") != 0){
        //convert the filepath to a non const to tokenize	
        char* nonConstFilePath = malloc(strlen(filePath)+1);
        if(!nonConstFilePath){
            return -1;
        }
        strcpy(nonConstFilePath,filePath);
        
        char* temp = nonConstFilePath;
        char** pathList = NULL;
        int count = 0;
        //set the delim for the string tok 
        const char *delim = "/";
        char* token;

        //count how many files we have in the path to know how much to malloc
        while(*temp){
            if(*temp == '/'){
                count++;
            }
            temp++;
        }
        //check if its jsut the root dir
        if(count == 0){
            //theres only a file or directory name so just add the count
            //and whats in there to one
            if((pathList = (char**)malloc(sizeof(char*)*2)) != NULL){
                //malloc the individual strings
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
            //loop through the toekens to be able to work
            while(token != NULL){
                if(strlen(token) > FNAME_MAX){
                    for(i = i - 1;i >= 0; i--){
                        free(pathList[i]);
                    }
                    //free everything so far if we get an error
                    free(pathList);
                    free(nonConstFilePath);
                    fprintf(stderr,"File or Dir name to long\n");
                    return -1;
                }
                //if all good malloc some stuff
                if((pathList[i] = (char*)malloc(strlen(token) + 1)) == NULL){
                    for(;i >= 0; i--){
                        free(pathList[i]);
                    }
                    free(pathList);
                    free(nonConstFilePath);
                    fprintf(stderr, "Error during mallocing\n");
                    return -1;
                }
                //copy the token into the newly allocated array
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
    //param check
    if(fs && fname && strcmp(fname,"") != 0 && dirInode >= 0){
        //set dir block to read into 
        dir_block_t dir;
        //read dir from blockstore
        if(block_store_read(fs->bs,fs->inodeTable[dirInode].data_ptrs[0],&dir,BLOCK_SIZE,0) == BLOCK_SIZE){
            if(dir.metaData.size < DIR_REC_MAX && strlen(fname) <= FNAME_MAX){
                //add everythign about the file to the dir
                dir.entries[dir.metaData.size].inode = fileInode;
                dir.entries[dir.metaData.size].ftype = ftype;
                strcpy(dir.entries[dir.metaData.size].filename, fname);
                //update the size of the dir
                dir.metaData.size++;
                //write the block back to the store
                if(block_store_write(fs->bs,fs->inodeTable[dirInode].data_ptrs[0],&dir,BLOCK_SIZE,0) == BLOCK_SIZE){
                    return 1;
                }
                fprintf(stderr,"failed to write to block afto din\n");
            }
            fprintf(stderr,"couldn't add to dir1\n");
            return -1;
        }
        fprintf(stderr,"coulnd't add to dir 2\n");
        return -1;
    }
    fprintf(stderr,"bad params addto din\n");
    return -1;
}


int fs_create_file(F15FS_t *const fs, const char *const fname, const ftype_t ftype){
    //param check
    if(fs && fname && strcmp(fname,"") != 0 && strcmp(fname,"/") != 0&& ftype){
        //get empty inode
        int emptyiNodeIndex = findEmptyInode(fs);
        if(emptyiNodeIndex < 0){
        	fprintf(stderr, "No empty Inodes\n");
        	return -1;
        }
        char **pathList = NULL;
        int listSize = 0;
        search_dir_t dirInfo;
        //parse filepath
        if(parseFilePath(fname,&pathList) > 0){
            //get the inode, shouldn't be able to find it 
            if(getInodeFromPath(fs,pathList, &dirInfo) == 0){
                listSize = (int)*pathList[0];
                //set the use byte
                fs->inodeTable[emptyiNodeIndex].metaData.inUse = 1;
                fs->inodeTable[emptyiNodeIndex].metaData.filetype = ftype;

                //add the fname to the inode
                strcpy(fs->inodeTable[emptyiNodeIndex].fname,pathList[listSize]);
                if(addFIleToDir(fs, pathList[listSize], emptyiNodeIndex,dirInfo.parentDir, ftype) < 0){
                	fprintf(stderr, "Not able to add file to Directory\n");
                	return -1;
                }
                //if its a dir set it up in the blockstore
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
    fprintf(stderr, "Bad params when creating the file\n");
    return -1;
}


int fs_get_dir(F15FS_t *const fs, const char *const fname, dir_rec_t *const records){
    if(fs && fname && fname[0] && records){
        char** pathList = NULL;
        search_dir_t dirInfo;
        dir_block_t dir;
        
        //this is kind of hacky but i'm running out of time
        //and this is the first time I have ever used a goto ever
        if(strcmp(fname,"/") == 0){
        	dirInfo.inode = 0;
        	goto RootSearch; 
        }
        //parse filepath
        if(parseFilePath(fname, &pathList) > 0){
            //get the indoe for the file or dir in this case
            if(getInodeFromPath(fs,pathList,&dirInfo) == -2){
                RootSearch:
                //read in teh dir
                if(block_store_read(fs->bs,fs->inodeTable[dirInfo.inode].data_ptrs[0],&dir,BLOCK_SIZE,0) == BLOCK_SIZE){
                    int i = 0;
                    records->total = dir.metaData.size;
                    //add all data to the records 
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

block_ptr_t writeDirectBLock(F15FS_t *const fs,size_t *dataLeftTOWrite, const void *data, size_t offset,size_t nbyte,size_t *needToAllocate, block_ptr_t blockId){
    if(fs && *dataLeftTOWrite > 0 && data && nbyte > 0){
        size_t dataOffset = nbyte - *dataLeftTOWrite;
        size_t bytesToWrite = 0;
        if(blockId > 65232){
        	return (uint32_t)-1;
        }
        //if we are overiding current data so no need to allocate
        if(BLOCK_IDX_VALID(blockId) && needToAllocate > 0){
            if(*dataLeftTOWrite < BLOCK_SIZE - offset){
                bytesToWrite = *dataLeftTOWrite;
            }
            else{
                bytesToWrite = BLOCK_SIZE - offset;
            }
            //write the data to the fs
            if(block_store_write(fs->bs,blockId,INCREMENT_VOID_PTR(data,dataOffset),bytesToWrite,offset) == bytesToWrite){
                *dataLeftTOWrite -= bytesToWrite;
                *needToAllocate -= bytesToWrite;
                return blockId;
            }
            fprintf(stderr,"failed to write to hd when writing direct block\n");
            exit(-1);
        }
        else{
        	//we need to allocate becasue we are writing new data
            blockId = block_store_allocate(fs->bs);

            if(*dataLeftTOWrite < BLOCK_SIZE - offset){
                bytesToWrite = *dataLeftTOWrite;
            }
            else{
                bytesToWrite = BLOCK_SIZE - offset;
            }
            //write the data to the blockstore
            if(block_store_write(fs->bs,blockId,INCREMENT_VOID_PTR(data,dataOffset),bytesToWrite,offset) == bytesToWrite){
                *dataLeftTOWrite -= bytesToWrite;
                return blockId;
            }
            fprintf(stderr,"failed to write to hd when writing direct block\n");
            exit(-1);
        }


    }
    fprintf(stderr,"Failed params check when writing direct block\n");
    exit(-1);
}

block_ptr_t writeIndirectBlock(F15FS_t *const fs,size_t *dataLeftTOWrite,const void *data,size_t nbyte,size_t needToAllocate,size_t blocksUsed,block_ptr_t indirectBlockId){
    if(fs && *dataLeftTOWrite > 0 && data && nbyte > 0){
        indirect_block_t indirectBlock;
        size_t bytesToWrite = 0;
        size_t i = 0;
        size_t CurrBlockToWrite = blocksUsed - DIRECT_TOTAL;

        //already alocated
        if(BLOCK_IDX_VALID(indirectBlockId) && CurrBlockToWrite > 0){
            //read in the indirect block to get the direct pointers
            if(block_store_read(fs->bs,indirectBlockId,&indirectBlock,BLOCK_SIZE,0) == BLOCK_SIZE){
                if(*dataLeftTOWrite < ((INDIRECT_TOTAL - CurrBlockToWrite - 1)*BLOCK_SIZE)){
                    bytesToWrite = *dataLeftTOWrite;
                }
                else{
                    bytesToWrite = ((INDIRECT_TOTAL - CurrBlockToWrite - 1)*BLOCK_SIZE);
                }
                size_t tempBytesToWrite = bytesToWrite;
                i = CurrBlockToWrite;
                //loop through to add to the direct blocks, call helper func
                while(bytesToWrite > 0){
                    indirectBlock.direct_ptr[i] = writeDirectBLock(fs,&bytesToWrite,data,0,nbyte,0,0);
                    if(indirectBlock.direct_ptr[i] == (uint32_t)-1){
                    	return (uint32_t)-1;
                    }
                    i++;
                }
                *dataLeftTOWrite -= tempBytesToWrite;
                return indirectBlockId;
            }
            fprintf(stderr, "Problem reading indirect block from hd\n");
            exit(-1);
        }
        else{
        	//the same as above but we now need to allocate everything 
            if(*dataLeftTOWrite < (INDIRECT_TOTAL*BLOCK_SIZE)){
                bytesToWrite = *dataLeftTOWrite;
            }
            else{
                bytesToWrite = INDIRECT_TOTAL * BLOCK_SIZE;
            }

            indirectBlockId = block_store_allocate(fs->bs);
            size_t tempBytesToWrite = bytesToWrite;
            while(bytesToWrite > 0){
                indirectBlock.direct_ptr[i] = writeDirectBLock(fs,&bytesToWrite,data,0,nbyte,0,0);
                i++;
            }
            *dataLeftTOWrite -= tempBytesToWrite;
            return indirectBlockId;
        }
    }
    fprintf(stderr, "failed param check while writing to indirect block\n");
    exit(-1);
}


ssize_t fs_write_file(F15FS_t *const fs, const char *const fname, const void *data, size_t nbyte, size_t offset){
    if(fs && fname && fname[0] && data && nbyte > 0 && (offset + nbyte) < FILE_SIZE_MAX){
        char **pathList = NULL;
        //int listSize = 0;
        size_t currFileSize = 0;
        size_t blocksUsed = 0;
        size_t needToAllocate = 0;
        size_t dataWriten = 0;
        //if something goes wrong it wont try and write set to zero
        size_t dataLeftTOWrite = 0;
        inode_ptr_t currFileIndex = 0;
        search_dir_t dirInfo;

        //parse file path
        if(parseFilePath(fname,&pathList) > 0){
        	//get inode for the file, it should exisit otherse error
            if(getInodeFromPath(fs,pathList, &dirInfo) > 0){
                //set up the
                // listSize = (int)*pathList[0];
                currFileIndex = dirInfo.inode;
                currFileSize = fs->inodeTable[currFileIndex].metaData.size;
                if(offset > currFileSize){
                    fprintf(stderr, "Trying to read more data than is in the file\n");
                    return -1;
                }
                //check when its time to start adding new blocks instead of writing over them
                needToAllocate = currFileSize - offset;
                dataLeftTOWrite = nbyte;
                dataWriten = dataLeftTOWrite;
                while(dataLeftTOWrite != 0){
                	//printf("data wrirte: %lu\n",dataLeftTOWrite);
                	blocksUsed = CURR_BLOCK_INDEX(offset);
                    //its time to read from the direct lbocks
                    if(blocksUsed >= 0 && blocksUsed < DIRECT_TOTAL){
                        fs->inodeTable[currFileIndex].data_ptrs[blocksUsed] = writeDirectBLock(fs,&dataLeftTOWrite,data,OFFSET_IN_BLOCK(offset),nbyte,&needToAllocate,fs->inodeTable[currFileIndex].data_ptrs[blocksUsed]);
                    	offset += (dataWriten - dataLeftTOWrite);
                    	dataWriten = dataLeftTOWrite;
                    }
                    //read from the indirect lbocks
                    else if(blocksUsed >= DIRECT_TOTAL && blocksUsed < INDIRECT_TOTAL){
                        fs->inodeTable[currFileIndex].data_ptrs[DIRECT_TOTAL] = writeIndirectBlock(fs,&dataLeftTOWrite,data,nbyte,needToAllocate,blocksUsed,fs->inodeTable[currFileIndex].data_ptrs[DIRECT_TOTAL]);
                    	offset += (dataWriten - dataLeftTOWrite);
                    	dataWriten = dataLeftTOWrite;
                    }
                    //read from double indirect
                    else if(blocksUsed >= INDIRECT_TOTAL && blocksUsed < DBL_INDIRECT_TOTAL){
                        fprintf(stderr,"have not implemented the ability for that big of a file\n");
                        return -1;
                        exit(-1);
                    }
                    //we messed up man
                    else{
                        fprintf(stderr,"file to big after already checked, so pretty weird error\n");
                        return -1;
                    }
                }
                fs->inodeTable[currFileIndex].metaData.size += nbyte;
                return nbyte;

            }
            fprintf(stderr,"Could not find file to write\n");
            return -1;
        }
        fprintf(stderr,"failed to parse filepath\n");
        return -1;
    }
    fprintf(stderr, "bad params while writing\n");
    return -1;
}




int readDirectBLock(F15FS_t *const fs,size_t *dataLeftTORead,const void *data, size_t offset, size_t nbyte, size_t blockId){
	if(fs && *dataLeftTORead > 0 && data && BLOCK_IDX_VALID(blockId)){
		size_t dataOffset = nbyte - *dataLeftTORead;
        size_t bytesToRead = 0;
        //set up the right data to stay in bounds 
        if(*dataLeftTORead < BLOCK_SIZE - offset){
            bytesToRead = *dataLeftTORead;
        }
        else{
            bytesToRead = BLOCK_SIZE - offset;
        }
        //read the data into the data buffer
        if(block_store_read(fs->bs,blockId,INCREMENT_VOID_PTR(data,dataOffset),bytesToRead,offset) == bytesToRead){
            *dataLeftTORead -= bytesToRead;
            return 1;
        }
        fprintf(stderr,"failed to write to hd when writing direct block\n");
        return -1;

	}
	fprintf(stderr,"Failed param check while reading direcet block\n");
	return -1;
}



ssize_t fs_read_file(F15FS_t *const fs, const char *const fname, void *data, size_t nbyte, size_t offset){
	if(fs && fname && fname[0] && data && nbyte > 0){
		//init vars
		char **pathList = NULL;
		search_dir_t dirInfo;
		size_t dataLeftTORead = 0;
		size_t currFileSize = 0;
        size_t blocksUsed = 0;
        size_t currFileIndex = 0;
        size_t dataRead = 0;

        //parse the file path
		if(parseFilePath(fname,&pathList) > 0){
			//get the indoe, it must exist
            if(getInodeFromPath(fs,pathList, &dirInfo) > 0){
            
            	currFileIndex = dirInfo.inode;
                currFileSize = fs->inodeTable[currFileIndex].metaData.size;
                if((offset + nbyte) > currFileSize){
                    fprintf(stderr, "Trying to read to big of a file\n");
                    return -1;
                }

                dataLeftTORead = nbyte;
                //keep trying as long as there is data to read
                while(dataLeftTORead != 0){
                	//printf("data wrirte: %lu\n",dataLeftTOWrite);
                	blocksUsed = CURR_BLOCK_INDEX(offset);
                	printf("blocksused = %lu\n",blocksUsed);
                    //rad from direct pointers
                    if(blocksUsed >= 0 && blocksUsed < DIRECT_TOTAL){
                        if(readDirectBLock(fs,&dataLeftTORead,data,OFFSET_IN_BLOCK(offset),nbyte,fs->inodeTable[currFileIndex].data_ptrs[blocksUsed]) < 0){
                        	fprintf(stderr,"Failed to read direct block while reading\n");
                        	return -1;
                        }

                    	offset += (dataRead - dataLeftTORead);
                    	dataRead = dataLeftTORead;
                    }
                    //rad from indirect poiters
                    else if(blocksUsed >= DIRECT_TOTAL && blocksUsed < INDIRECT_TOTAL){
                        fprintf(stderr,"Not yet implemented\n");
                        return -1;
                     //    if(readIndirectBlock(fs,&dataLeftTORead,data,nbyte,blocksUsed,fs->inodeTable[currFileIndex].data_ptrs[DIRECT_TOTAL]) < 0){
                     //    	fprintf(stderr,"Failed to read indirect block while reading file\n");
                     //    }
                    	// offset += (dataRead - dataLeftTORead);
                    	// dataRead = dataLeftTORead;
                    }
                    //read from double indirect pointers
                    else if(blocksUsed >= INDIRECT_TOTAL && blocksUsed < DBL_INDIRECT_TOTAL){
                        fprintf(stderr,"have not implemented the ability for that big of a file\n");
                        return -1;
                        exit(-1);
                    }
                    else{
                        fprintf(stderr,"file to big after already checked, so pretty weird error\n");
                        return -1;
                    }
                }


            }
            fprintf(stderr,"Failed to find file while reading\n");
        }
        fprintf(stderr,"Failed to parse file path while reading file\n");
	}	return -1;
	fprintf(stderr, "Failed params while reading File\n");
	return -1;
}

