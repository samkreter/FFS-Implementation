#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int parseFilePath(const char *const filePath, char** pathListOutput);

int main(){


    const char *const path = "/folder/withfilethatiswayyyyytoolongwhydoyoumakefilesthataretoobig";



    char **test = NULL;
    if(parseFilePath(path, test)){



        printf("%d\n",(int)(*test[0]));
        int size = (int)(*test[0]);
        int i = 1;
        for(;i<size+1;i++){
            printf("%s\n",test[i]);
        }
    }

    return 0;
}

int parseFilePath(const char *const filePath, char** pathListOutput){
    if(filePath && strcmp(filePath,"") != 0){
        char* nonConstFilePath = malloc(strlen(filePath)+1);
        if(!nonConstFilePath){
            return 0;
        }
        strcpy(nonConstFilePath,filePath);
        char* temp = nonConstFilePath;
        char** pathList = NULL;
        int count = 0;
        int i = 1;
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
                    pathListOutput = pathList;
                    return 1;
                }
                free(pathList);
            }
            free(nonConstFilePath);
            fprintf(stderr, "error during mallocing\n");
            return 0;
        }else{
            //create string array with right size plus one to add the size in
            if((pathList = (char**)malloc(sizeof(char*)*(count+1))) == NULL){
                free(nonConstFilePath);
                fprintf(stderr, "error during mallocing\n");
                return 0;
            }

            if((pathList[0] = malloc(sizeof(char))) == NULL){
                free(nonConstFilePath);
                free(pathList);
                fprintf(stderr, "error during mallocing\n");
                return 0;
            }

            //put the length at the begging
            *pathList[0] = count;

            token = strtok(nonConstFilePath,delim);


            while(token != NULL){
                if((pathList[i] = (char*)malloc(strlen(token))) == NULL){
                    free(nonConstFilePath);
                    free(pathList);
                    fprintf(stderr, "error during mallocing\n");
                    return 0;
                }

                strcpy(pathList[i],token);

                token = strtok(NULL,delim);
                i++;
            }
            free(nonConstFilePath);
            pathListOutput = pathList;
            return 1;

        }
    }
    fprintf(stderr, "bad params\n");
    return 0;
}