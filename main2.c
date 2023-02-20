#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct __attribute__((__packed__)) {
    uint8_t BS_jmpBoot[ 3 ]; // x86 jump instr. to boot code
    uint8_t BS_OEMName[ 8 ]; // What created the filesystem
    uint16_t BPB_BytsPerSec; // Bytes per Sector
    uint8_t BPB_SecPerClus; // Sectors per Cluster
    uint16_t BPB_RsvdSecCnt; // Reserved Sector Count
    uint8_t BPB_NumFATs; // Number of copies of FAT
    uint16_t BPB_RootEntCnt; // FAT12/FAT16: size of root DIR
    uint16_t BPB_TotSec16; // Sectors, may be 0, see below
    uint8_t BPB_Media; // Media type, e.g. fixed
    uint16_t BPB_FATSz16; // Sectors in FAT (FAT12 or FAT16)
    uint16_t BPB_SecPerTrk; // Sectors per Track
    uint16_t BPB_NumHeads; // Number of heads in disk
    uint32_t BPB_HiddSec; // Hidden Sector count
    uint32_t BPB_TotSec32; // Sectors if BPB_TotSec16 == 0
    uint8_t BS_DrvNum; // 0 = floppy, 0x80 = hard disk
    uint8_t BS_Reserved1; //
    uint8_t BS_BootSig; // Should = 0x29
    uint32_t BS_VolID; // 'Unique' ID for volume
    uint8_t BS_VolLab[ 11 ]; // Non zero terminated string
    uint8_t BS_FilSysType[ 8 ]; // e.g. 'FAT16 ' (Not 0 term.)
} BootSector;

typedef struct __attribute__((__packed__)){
    uint8_t DIR_Name[11];   //non zero terminated string
    uint8_t DIR_Attr;       //file attributes
    uint8_t DIR_NTRes;     //used by windows NT, ignore
    uint8_t DIR_CrtTimeTenth; //tenths of sec. 0..199
    uint16_t DIR_CrtTime;     //creation time in 2s intervals
    uint16_t DIR_CrtDate;     //date file created
    uint16_t DIR_LstAccDate;  //date of last read or write
    uint16_t DIR_FstClusHI;  //top 16 bits file's 1st cluster
    uint16_t DIR_WrtTime;   // time of last write
    uint16_t DIR_WrtDate;   // date of last write
    uint16_t DIR_FstClusLO;  //lower 16 bits file's 1st cluster
    uint32_t DIR_FileSize;   //file size in bytes
 
} DirectoryEntry;


typedef struct __attribute__((__packed__)){
    uint8_t LDIR_Ord;           // Order/ position in sequence/ set
    uint8_t LDIR_Name1[10];     // First 5 UNICODE characters
    uint8_t LDIR_Attr;          // = ATTR_LONG_NAME (xx001111)
    uint8_t LDIR_Type;          // Should = 0
    uint8_t LDIR_Chksum;        // Checksum of short name
    uint8_t LDIR_Name2[12];     // Middle 6 UNICODE characters
    uint16_t LDIR_FstClusLO;    // MUST be zero 
    uint8_t LDIR_Name3[4];      // Last 2 UNICODE characters
} LongDirectory;

BootSector* bootP;
DirectoryEntry* dirEnt;
uint16_t* fatF;
uint32_t dirEntEnd;

void readSectors(int file, void * buffer, unsigned int start, unsigned int length ){

    int offset = (bootP->BPB_BytsPerSec)*start;
    int bytesToRead = (bootP->BPB_BytsPerSec) * length;
    
    lseek(file,offset,SEEK_SET);
    read(file,buffer,bytesToRead);

} 

void readFile(int clusterLO, int file){

    int startOfFat = bootP->BPB_RsvdSecCnt*bootP->BPB_BytsPerSec;
    int startOfDirEnt = startOfFat + bootP->BPB_NumFATs*bootP->BPB_FATSz16*bootP->BPB_BytsPerSec;
    int startOfData = startOfDirEnt + sizeof(DirectoryEntry)*bootP->BPB_RootEntCnt;
    int size = bootP->BPB_BytsPerSec*bootP->BPB_SecPerClus;
    int currentCluster = clusterLO; 
 
    while(currentCluster < 0xfff8){
    
        char* buffer = (char*)malloc(sizeof(char)*bootP->BPB_SecPerClus*bootP->BPB_BytsPerSec);
        int offset = startOfData+( (currentCluster-2)*size);    
        int bytesToRead = size;
        lseek(file,offset,SEEK_SET);
        read(file,buffer,bytesToRead);
        printf("%s",buffer);
        currentCluster = fatF[currentCluster];
        free(buffer);
    }
    printf("\n");

}

void readRootDirectory(int file){
    //task 4 -read root directory
    //uint32_t start = bootP->BPB_RsvdSecCnt + bootP->BPB_NumFATs*bootP->BPB_FATSz16;
     uint32_t start = bootP->BPB_RsvdSecCnt + bootP->BPB_NumFATs*bootP->BPB_FATSz16;
     uint16_t size = sizeof(DirectoryEntry) * bootP->BPB_RootEntCnt;
     uint16_t sectors = (size / bootP->BPB_BytsPerSec);
     if( size % bootP->BPB_BytsPerSec > 0){
         sectors++; //round up
     }
    // dirEntEnd = bootP->BPB_RsvdSecCnt*bootP->BPB_BytsPerSec + bootP->BPB_NumFATs*bootP->BPB_FATSz16*bootP->BPB_BytsPerSec; //where root directory ends
    // printf("%u\n",dirEntEnd);
    dirEnt = (DirectoryEntry*)malloc(sectors*bootP->BPB_BytsPerSec); //cos may have been rounded down
    readSectors(file,dirEnt,start,sectors);
}

void accessFAT(int file){
    fatF = (uint16_t*)malloc(bootP->BPB_BytsPerSec*bootP->BPB_FATSz16);
    readSectors(file,fatF,bootP->BPB_RsvdSecCnt,bootP->BPB_FATSz16);
    
}

void readBoot(int file){
        bootP = (BootSector*)malloc(sizeof(BootSector));
        int offSet = 0;
        lseek(file,offSet,SEEK_SET);
        read(file,bootP,sizeof(BootSector));

}

void readLongDir(int count,DirectoryEntry * entries){

    if(entries[count].DIR_Attr!=15){
        
        return;
    }

    else{
            LongDirectory longDir;
            memcpy(&longDir, &entries[count], sizeof(longDir));
               
            char name1Temp[5];
            char name2Temp[6];
            char name3Temp[2];
            int index=0;
            
            char temp[5*6*2];
            
            for(int i=0;i<10;i=i+2){
                if (longDir.LDIR_Name1[i] == 0xFF) break;
                temp[index]=longDir.LDIR_Name1[i];
                index++;
            }
            
            for(int i=0;i<12;i=i+2){
                if (longDir.LDIR_Name2[i] == 0xFF) break;
                temp[index]=longDir.LDIR_Name2[i];
                index++;
            }
         
            for(int i=0;i<4;i=i+2){
                if (longDir.LDIR_Name3[i] == 0xFF) break;
                temp[index]=longDir.LDIR_Name3[i];
                index++;
            }
            printf("%s",temp);
            readLongDir(count-1,entries);

    }


}

void readDirFiles(int file,int firstCluster){

    int startOfFat = bootP->BPB_RsvdSecCnt*bootP->BPB_BytsPerSec;
    int startOfDirEnt = startOfFat + bootP->BPB_NumFATs*bootP->BPB_FATSz16*bootP->BPB_BytsPerSec;
    int startOfData = startOfDirEnt + sizeof(DirectoryEntry)*bootP->BPB_RootEntCnt;
    int currentCluster =firstCluster; 
    int offset; 
    int size = bootP->BPB_BytsPerSec*bootP->BPB_SecPerClus;
    DirectoryEntry * subFile;
    
    int count =0;  
    while(currentCluster< 0x0fff8){ //have to loop twice first to get count
        currentCluster = fatF[currentCluster];
        count++;

    }

    DirectoryEntry * dirEntryFiles = (DirectoryEntry*)malloc(size*count);
    currentCluster=firstCluster; //change back entry->DIR_LO
    int index=0;
    while(currentCluster< 0x0fff8){ //have to loop twice first to get count
        offset = startOfData+( (currentCluster-2)*size); //get 64 entries
        subFile = (DirectoryEntry*)malloc(size);
        lseek(file,offset,SEEK_SET);
        read(file,subFile,size);

        currentCluster = fatF[currentCluster];
        for(int i=0;i<64;i++){
            dirEntryFiles[index]=subFile[i];
            index++;
        }

        free(subFile);
       
    }

     for(int i=0;i<count*64;i++){ //not printing every file in loop but is up here because have continue
        //for each file

        // if first byte in directory name is zero or byte is 0xE5 then invalid file
        if(dirEntryFiles[i].DIR_Name[0] == 0 || dirEntryFiles[i].DIR_Name[0]==0xE5){
            //ignore

             //do nothing
        }

        else if(dirEntryFiles[i].DIR_Attr==32){   //bits[3]==0 && bits[4]==0

            printf("%2d:%d:%d  ||",(dirEntryFiles[i].DIR_WrtDate >> 11),(dirEntryFiles[i].DIR_WrtDate>>5) & 0b111111,((dirEntryFiles[i].DIR_WrtDate) & 0b11111 )*2 );
            printf("%d/%d/%d  ||",(dirEntryFiles[i].DIR_LstAccDate) & 0b11111,(dirEntryFiles[i].DIR_LstAccDate>>5)&0b1111,1980+(dirEntryFiles[i].DIR_LstAccDate>>9) );
            printf("A-----  ||");
            printf("%u bytes  ||",dirEntryFiles[i].DIR_FileSize);
            printf("%u  ||",dirEntryFiles[i].DIR_FstClusLO);
            printf("%s ",dirEntryFiles[i].DIR_Name);
             
           if(dirEntryFiles[i-1].DIR_Attr==15){ //if one before is a long file
                
                readLongDir(i-1,dirEntryFiles);
           }
           printf("\n");

        }
        else if(dirEntryFiles[i].DIR_Attr==8){

            printf("%2d:%d:%d ||",(dirEntryFiles[i].DIR_WrtDate >> 11),(dirEntryFiles[i].DIR_WrtDate>>5) & 0b111111,((dirEntryFiles[i].DIR_WrtDate) & 0b11111 )*2 );
            printf("%d/%d/%d  ||",(dirEntryFiles[i].DIR_LstAccDate) & 0b11111,(dirEntryFiles[i].DIR_LstAccDate>>5)&0b1111,1980+(dirEntryFiles[i].DIR_LstAccDate>>9) );
            printf("--V---  ||");
            printf("%u bytes  ||",dirEntryFiles[i].DIR_FileSize);
            printf("%u  ||",dirEntryFiles[i].DIR_FstClusLO);
            printf("%s \n",dirEntryFiles[i].DIR_Name);
  
        }
        
        else if(dirEntryFiles[i].DIR_Attr==16) {
             
            if(dirEntryFiles[i].DIR_Name[0]=='.'){
                //ignore
            } 
            else{

                printf("%2d:%d:%d || ",(dirEntryFiles[i].DIR_WrtDate >> 11),(dirEntryFiles[i].DIR_WrtDate>>5) & 0b111111,((dirEntryFiles[i].DIR_WrtDate) & 0b11111 )*2 );
                printf("%d/%d/%d  || ",(dirEntryFiles[i].DIR_LstAccDate) & 0b11111,(dirEntryFiles[i].DIR_LstAccDate>>5)&0b1111,1980+(dirEntryFiles[i].DIR_LstAccDate>>9) );
                printf("-D---- ||");
                printf("%u bytes  ||",dirEntryFiles[i].DIR_FileSize);
                printf("%u   ||",dirEntryFiles[i].DIR_FstClusLO);
                printf("%s \n",dirEntryFiles[i].DIR_Name);
                
                readDirFiles(file,dirEntryFiles[i].DIR_FstClusLO);
            
            }
            
        }
        
    }
    
    free(dirEntryFiles);
    return;
    
}

void readEachFile(int file,DirectoryEntry* dirEntries,int count){

     //for every file
    for(int i=0;i<count;i++){ 
        //for each file

        // if first byte in directory name is zero or byte is 0xE5 then invalid file
        if(dirEntries[i].DIR_Name[0] == 0 || dirEntries[i].DIR_Name[0]==0xE5){
            //ignore

             //do nothing
        }

        if(dirEntries[i].DIR_Attr==32){   //bits[3]==0 && bits[4]==0
           
            printf("%2d:%d:%d  ||",(dirEntries[i].DIR_WrtDate >> 11),(dirEntries[i].DIR_WrtDate>>5) & 0b111111,((dirEntries[i].DIR_WrtDate) & 0b11111 )*2 );
            printf("%d/%d/%d  ||",(dirEntries[i].DIR_LstAccDate) & 0b11111,(dirEntries[i].DIR_LstAccDate>>5)&0b1111,1980+(dirEntries[i].DIR_LstAccDate>>9) );
            printf("A-----  ||");
            printf("%u bytes  ||",dirEntries[i].DIR_FileSize);
            printf("%u  ||",dirEntries[i].DIR_FstClusLO);
            printf("%s ",dirEntries[i].DIR_Name);

           if(dirEntries[i-1].DIR_Attr==15){ 
                
                readLongDir(i-1,dirEntries);
           }
           
           printf("\n");
           
        }
        else if(dirEntries[i].DIR_Attr==8){

            printf("%2d:%d:%d ||",(dirEntries[i].DIR_WrtDate >> 11),(dirEntries[i].DIR_WrtDate>>5) & 0b111111,((dirEntries[i].DIR_WrtDate) & 0b11111 )*2 );
            printf("%d/%d/%d  ||",(dirEntries[i].DIR_LstAccDate) & 0b11111,(dirEntries[i].DIR_LstAccDate>>5)&0b1111,1980+(dirEntries[i].DIR_LstAccDate>>9) );
            printf("--V---  ||");
            printf("%u bytes  ||",dirEntries[i].DIR_FileSize);
            printf("%u  ||",dirEntries[i].DIR_FstClusLO);
            printf("%s \n",dirEntries[i].DIR_Name);
  
        }
        
        else if(dirEntries[i].DIR_Attr==16) {
        
            if(dirEntries[i].DIR_Name[0]=='.'){
                //ignore
            } 
            else{

                printf("%2d:%d:%d || ",(dirEntries[i].DIR_WrtDate >> 11),(dirEntries[i].DIR_WrtDate>>5) & 0b111111,((dirEntries[i].DIR_WrtDate) & 0b11111 )*2 );
                printf("%d/%d/%d  || ",(dirEntries[i].DIR_LstAccDate) & 0b11111,(dirEntries[i].DIR_LstAccDate>>5)&0b1111,1980+(dirEntries[i].DIR_LstAccDate>>9) );
                printf("-D---- ||");
                printf("%u bytes  ||",dirEntries[i].DIR_FileSize);
                printf("%u   ||",dirEntries[i].DIR_FstClusLO);
                printf("%s \n",dirEntries[i].DIR_Name);

                readDirFiles(file,dirEntries[i].DIR_FstClusLO); 
                
            }
            
        }
        
    }
    
}

int main(){
    
    int file = open("fat16.img",O_CREAT);
    readBoot(file);
    accessFAT(file);
    readRootDirectory(file);
    printf("LAST MODIFIED | LAST OPENED |  TYPE  |  SIZE(BYTES) |  CLUSTER LO  |  NAME | \n");
    readEachFile(file,dirEnt,64); //main directory

    while(1){
        int clus;
        printf("Input a cluster number or enter -1 to bring up table again: ");
        scanf("%d",&clus);
        if(clus==-1){
            printf("\n");
            readEachFile(file,dirEnt,bootP->BPB_RootEntCnt);
        }
        else{
            readFile(clus,file);
        }
        
    }
    
    close(file);
    
    return 0;
}






