#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<errno.h>
#include<time.h>

#define MAX_BLOCK_COUNT 100
#define MAX_BLOCK_SIZE 10
#define FREE 1
#define OCCUPIED 0
#define MAXFILENAMESIZE 256
#define INPUTFILESIZE 70
#define MAXBLOCKPERFILE 4
#define MAX_FILE_SUPPORTED 1000
#define MAX_INDIRECT_PER_INODE 1
#define MAX_INODE_IN_DIR 50
#define MAX_DIR_IN_DIR 10
#define ROOT "root_dir"


/** This is memory Back store **/
char  memoryBlock[MAX_BLOCK_COUNT][MAX_BLOCK_SIZE+1];


/** This structure will store list of index for free memory **/
struct ListFreeIndex{
        int totalFreeCount;
        int isFree[MAX_BLOCK_COUNT];
};

struct ListFreeIndex FreeMap;

/** This structure is used for indirect access **/
struct IndirectIndexNode{
        int maxIndexFreeDirect;
        int indexOfDirectMemory[MAXBLOCKPERFILE];
};

/** This is structure for inode **/
struct IndexNode{
        int blocksOccupied;
        int fileSize;
        int maxIndexFreeDirect;
        int maxIndexFreeIndirect;
        char fileName[MAXFILENAMESIZE];
        int indexOfDirectMemory[MAXBLOCKPERFILE];
        struct IndirectIndexNode indexOfInDirectMemory[MAX_INDIRECT_PER_INODE];
};


/** This structure store information regarding directory **/
struct Directory{
        int maxFreeInodeIndex;
        int maxFreeChildDirIndex;
        char dirName[MAXFILENAMESIZE];
        int inode_no[MAX_INODE_IN_DIR];
        struct Directory *child_dir[MAX_DIR_IN_DIR];
};

struct Directory root;

/** This will act as inode-table **/
struct InodeTable{
        struct IndexNode *inode;
        int inode_no;
};

struct InodeTable inodeTable[MAX_FILE_SUPPORTED];

int currentFreeInodeTableIndex = 0;
int CurrInodeNo=0;

void initialiseDir(struct Directory *dir, char *dirName){
        int i=0;
        dir->maxFreeInodeIndex=0;
        dir->maxFreeChildDirIndex=0;
        strcpy(dir->dirName, dirName);
        for(i=0; i<MAX_INODE_IN_DIR; i++){
                dir->inode_no[i]=-1;
                dir->child_dir[i]=NULL;
        }
}

struct Directory *searchExistingDir(char *dirName, struct Directory *thisDir){
        struct Directory *myDir=NULL;

        if(thisDir==NULL)
                return NULL;

        int i=0;

        if(strcmp(thisDir->dirName, dirName)==0){
                return thisDir;
        }

        for(i=0; i<(thisDir->maxFreeChildDirIndex); i++){
                myDir=searchExistingDir(dirName, thisDir->child_dir[i]);
                if(myDir!=NULL)
                        break;
        }
        return myDir;
}

int mapFileToInodeTable(struct IndexNode *fileIndex, char *dirName){
        struct Directory *dirCurr = searchExistingDir(dirName, &root);
        if(dirCurr==NULL){
                fprintf(stderr, "Directory does not exist");
                return 1;
        }

        if((dirCurr->maxFreeInodeIndex) <= MAX_INODE_IN_DIR){
                inodeTable[currentFreeInodeTableIndex].inode=fileIndex;
                inodeTable[currentFreeInodeTableIndex++].inode_no=CurrInodeNo;
                dirCurr->inode_no[dirCurr->maxFreeInodeIndex] = CurrInodeNo++;
                (dirCurr->maxFreeInodeIndex)++;
        }else {
                fprintf(stderr, "Max nested DIR limit is reached for dir [%s]", dirName);
                return 1;
        }
        return 0;
}


int createFile(char *fileName){
        FILE *fp=NULL;
        int i=0, j=0, counter=0;
        fp = fopen("newFile.txt", "w");

        if(fp==NULL){
                fprintf(stderr, "Error while opening newFile.txt: %s", strerror(errno));
                return 1;
        }

        for(i=0; i<(INPUTFILESIZE/10)+1; i++){
                for(j=0; j<10; j++){
                         if(counter>=INPUTFILESIZE)
                                break;
                        fprintf(fp, "%d", j);
                        counter++;
                }
        }
        fclose(fp);
        return INPUTFILESIZE;
}

void initialise(struct IndexNode** node, char *fileName){
        int index=0, j=0;
        (*node)->fileSize=0;
        (*node)->blocksOccupied=0;
        (*node)->maxIndexFreeDirect=0;
        (*node)->maxIndexFreeIndirect=0;
        strncpy((*node)->fileName, fileName, MAXFILENAMESIZE-1);
        (*node)->fileName[MAXFILENAMESIZE]='\0';
        for(index=0; index<MAXBLOCKPERFILE; index++){
                (*node)->indexOfDirectMemory[index] = 0;
        }

        for(index=0; index<MAX_INDIRECT_PER_INODE; index++){
                (*node)->indexOfInDirectMemory[index].maxIndexFreeDirect =0;
                for(j=0; j<MAXBLOCKPERFILE; j++)
                (*node)->indexOfInDirectMemory[index].indexOfDirectMemory[j]=0;
        }
}

int getRandomFreeIndex(){
        srand(time(NULL));
        int i=0;
        int freeIndex=-1;
        int r = rand()%(FreeMap.totalFreeCount);

        for(i=0; freeIndex<=r;  i++){
                if(FreeMap.isFree[i]==FREE){
                        freeIndex++;
                }
        }

        if(i>=r)
                return i-1;
        else
                return -1;
}

struct IndexNode *loadFileInMemory(char *fileName, int fileSize){
        int randomFreeIndex=-1;
        char buffer[11];
        FILE *fp =NULL;
        int dataRead=0;
        struct IndexNode *newNode = NULL;

        fp = fopen(fileName, "r");

        if(fp==NULL){
                fprintf(stderr, "Error in file opneing: %s", strerror(errno));
                return NULL;
        }

        if(strlen(fileName)>=MAXFILENAMESIZE-1){
                fprintf(stderr, "Length of FileName [%s] is exceeding the limit", fileName);
                return NULL;
        }

        do{
                if(newNode==NULL){
                        newNode = (struct IndexNode *)malloc(sizeof(struct IndexNode));

                        /** initialise structure **/
                        initialise(&newNode, fileName);
                }

                /**if((newNode->fileSize)>=(MAXBLOCKPERFILE*MAX_BLOCK_SIZE)){
                        fprintf(stderr, "File size is exceeded for file %s", fileName);
                        return newNode;
                }**/

                srand(time(NULL));
                randomFreeIndex = getRandomFreeIndex();
                memset(memoryBlock[randomFreeIndex], '\0', sizeof(char)*(MAX_BLOCK_SIZE+1));
                dataRead=fread(memoryBlock[randomFreeIndex], MAX_BLOCK_SIZE, 1, fp);
                if(memoryBlock[randomFreeIndex][0]!='\0'){
                        memoryBlock[randomFreeIndex][MAX_BLOCK_SIZE] = '\0';
                        printf("Data Readed: [%s]\n", memoryBlock[randomFreeIndex]);

			/** Direct allocation **/
                        if((newNode->maxIndexFreeDirect)<=MAXBLOCKPERFILE){
                                (newNode)->indexOfDirectMemory[newNode->maxIndexFreeDirect]=randomFreeIndex;
                                (newNode->maxIndexFreeDirect)++;
                        }
                        /** indirect allocation **/
                        else if((newNode->maxIndexFreeIndirect)<=MAX_INDIRECT_PER_INODE){
                                newNode->indexOfInDirectMemory[newNode->maxIndexFreeIndirect].indexOfDirectMemory[newNode->indexOfInDirectMemory[newNode->maxIndexFreeIndirect].maxIndexFreeDirect]=randomFreeIndex;
                                (newNode->indexOfInDirectMemory[newNode->maxIndexFreeIndirect].maxIndexFreeDirect)++;
				(newNode->maxIndexFreeIndirect)++;
                        }else{
                                fprintf(stderr, "File size is exceeded for file %s", fileName);
                                fclose(fp);
                                return newNode;
                        }

                        /** adjust data structures **/
                        FreeMap.totalFreeCount--;
                        FreeMap.isFree[randomFreeIndex]=OCCUPIED;
                        (newNode->fileSize)+=MAX_BLOCK_SIZE;
                        (newNode->blocksOccupied)++;

                }
        }while(dataRead>0);

        fclose(fp);

        return newNode;
}

void readAndPrint(struct IndexNode *inode){
        int index=0;
        printf("\n");
        for(index=0; index<(inode->maxIndexFreeDirect); index++)
        {
                printf("%s", memoryBlock[(inode)->indexOfDirectMemory[index]]);
        }
        printf("\n");
        fflush(stdout);
}


int main(){
        int fileSize=0, i=0;

        /** initialise whole memory as free **/
        FreeMap.totalFreeCount=MAX_BLOCK_COUNT;
        for(i=0; i<MAX_BLOCK_COUNT; i++){
                FreeMap.isFree[i]=FREE;
        }

        /** create file of mentione size **/
        fileSize=createFile("newFile.txt");
        if(fileSize==0){
                return 1;
        }

        /** read file and store in memory back store **/
        struct IndexNode *fileIndex = loadFileInMemory("newFile.txt", fileSize);

        if(fileIndex==NULL){
                return 1;
        }

        /** read file from memory and print **/
        readAndPrint(fileIndex);

        /** initialise root directory **/
        initialiseDir(&root, ROOT);

        /** initialise inode-table **/
        for(i=0; i<MAX_FILE_SUPPORTED; i++)
        {
                inodeTable[MAX_FILE_SUPPORTED].inode=NULL;
                inodeTable[MAX_FILE_SUPPORTED].inode_no=-1;
        }

        /** map file to directory and inode **/
        if(mapFileToInodeTable(fileIndex, ROOT)==1)
                return 1;

        return 0;
}
