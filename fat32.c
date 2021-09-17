//-----------------------------------------
// NAME: 			Alyssa Carungay
// STUDENT NUMBER: 	7725178
// COURSE: 			COMP 3430, SECTION: A02
// INSTRUCTOR: 		Franklin
// ASSIGNMENT: 		A4 Q1
//-----------------------------------------
// This application reads in FAT32 disk image and performes one of three tasks:
//		1) info: 	list info about the FAT32 volume
//		2) list: 	list all directory entires on the FAT32 volume
//		3) get: 	gets given file and stores in the same directory 
//					volume resides in
//-----------------------------------------
#define _FILE_OFFSET_BITS 64
#define SECT_SIZE 512

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include "fat32.h"
#include "fat32Dir.h"
#include "fsInfo.h"

//-------------------------------
// GLOBAL VARIABLES
//-------------------------------
int fd;						// file descriptor for disk image file
int new_file;				// file descriptor for file being written with 'get'command
int endOS;					// indicates end of search path
fat32BootSector bpb;		// Boot Sector 
fat32FSInfo fsi;			// File Info Sector
unsigned char dirname[13];	// store directory name

uint16_t bytes_PER_sec;		// # bytes / sector
uint8_t sectors_PER_clus;	// # sectors / cluster
uint8_t dirEntry_PER_clus;	// # directory entries / cluster
uint16_t RootDirSectors;	// number of sectors for Root diretory (0 in FAT32)
uint16_t rsvd_sectors;		// number of reserved sectors 
uint32_t filetable_start;	// sector where FAT is
uint32_t root_dir;			// cluster where root directory 
uint32_t data_region;		// cluster where data region begins

//--------------------------------
// FUNCTIONS:
//--------------------------------
void getInfo();
void printInfo();
void list(uint32_t cluster, int level);
void getFile(char* filename);
uint32_t  checkFAT(uint32_t cluster_num);
uint32_t find_sector(uint32_t cluster_num);
uint32_t  findDirectory(uint32_t folder, char* path);
void writeFile(int32_t cluster);
void getname(unsigned char name[], int level);
void printlevel(int level);
int validCHAR(unsigned char c);
int compare(char* str);
uint32_t countFreeClus();

//--------------------------------
// WRAPPER FUNCTIONS:
//--------------------------------
int readfile(int fd, unsigned char buffer[], int size);
int writef(int fd, unsigned char buffer[], int size);
int seek(int fd, int offset, int whence);


//------------------------------------------------------
// Main()
//------------------------------------------------------
// Reads in command line and starts running desired input
//	
// PARAMETERS:
// argv[1] -> the FAT32 disk image
// argv[2] -> task to perform on FAT32 Volume
//		info: ound robin
// 		list: shortest time to complete first
// 		get : multi level queue
// argv[3] -> the filepath for file to get
//------------------------------------------------------
int main(int argc, char* argv[])
{
	if(argc < 3 || argc > 4)
	{
		printf("Check usage: \n./fat32 [disk img] [info/list/get] [filepath (only for get)]\n\n");
		exit(1);
	} 

	char* diskimg = argv[1];
	char* cmd = argv[2];

	//open file descriptor to diskimage
	fd = open(diskimg, O_RDONLY);
	if(fd < 0)
	{
		printf("Failed to open disk image\n");
	}

	//read in data structures
	getInfo();

	//determine what command to run
	if(strcmp(cmd,"info") == 0 && argc == 3)
	{
		printInfo();
	}
	else if(strcmp(cmd,"list") == 0 && argc == 3)
	{
		list(bpb.BPB_RootClus, 0);
	}
	else if(strcmp(cmd,"get") == 0)
	{
		if(argc == 4)
		{
			getFile(argv[3]);
		}
		else
		{
			printf("'get' requires filename as arg\n");
		}
	}
	else
	{
		printf("Check usage\n");
		exit(1);
	}

	close(fd);
	close(new_file);

	return 0;
}

//----------------------------------------------------------------------
// getInfo()
//----------------------------------------------------------------------
// Gets the boot sector and FSI sectors of the FAT32 volume and stores 
// them into data structures
// Sets locations of the root, FAT and data region also 
// stores sizes of secotrs, clusters, reserved section
//----------------------------------------------------------------------
void getInfo()
{
	//copy bytes to bootsector
	unsigned char buffer[SECT_SIZE];	//read in entire boot sector
	readfile(fd, buffer, SECT_SIZE);
	memcpy(&bpb,buffer,sizeof(fat32BootSector));
	
	//get sizes
	bytes_PER_sec = bpb.BPB_BytesPerSec;
	sectors_PER_clus = bpb.BPB_SecPerClus;
	RootDirSectors = ((bpb.BPB_RootEntCnt*32) + (bpb.BPB_BytesPerSec-1)) / bpb.BPB_BytesPerSec;
	rsvd_sectors = (bpb.BPB_FATSz32 * bpb.BPB_NumFATs) + bpb.BPB_RsvdSecCnt + RootDirSectors;
	dirEntry_PER_clus = bytes_PER_sec / sizeof(fat32DirEntry);

	//get important sector locations
	filetable_start = bpb.BPB_RsvdSecCnt;
	root_dir = bpb.BPB_RootClus;
	data_region = rsvd_sectors;

	if(bpb.BPB_FATSz32 == 0)
	{
		printf("Error: Not a FAT32 disk\n");
		exit(1);
	}

	if(bpb.BS_BootSig != 0x29)
	{
		printf("Error: Volume ID, Volume label and File system type not present\n");
		exit(1);
	}


	//read in FSI
	int fsi_sector = bytes_PER_sec * bpb.BPB_FSInfo;
	seek(fd,fsi_sector,SEEK_SET);
	readfile(fd, buffer, SECT_SIZE);
	memcpy(&fsi,buffer,sizeof(fat32FSInfo));

	//check FSInfo signatures
	if(fsi.FSI_LeadSig != 0x41615252)
	{
		printf("This is not a FSInfo Sector\n");
	}
	if(fsi.FSI_StructSig != 0x61417272)
	{
		printf("This is not a FSInfo Sector\n");
	}
	if(fsi.FSI_TrailSig != 0xAA550000)
	{
		printf("This is not a FSInfo Sector\n");
	}
}

//----------------------------------------------------------------------
// countFreeClus()
//----------------------------------------------------------------------
// Calculates the amount of free data clusters in the FAT32 volume
//
// RETURN:
//		int freeCount:	Number of free data clusters
//----------------------------------------------------------------------
uint32_t countFreeClus()
{
	uint32_t fatEntry_size = 4;		// number of bytes in a fat entry
	uint32_t maxEnt_sector = bytes_PER_sec / fatEntry_size; // # entries per sector
	unsigned char buffer[fatEntry_size];

	uint32_t entRead = 0;				// number of read entries / cluster
	int freeCount = 0;					// number of free entries
	
	uint32_t sectStart = filetable_start * bytes_PER_sec;
	seek(fd,sectStart,SEEK_SET);

	while(entRead < bpb.BPB_FATSz32 * maxEnt_sector)
	{
		uint32_t fatEntry;		// holds fat entry
		readfile(fd, buffer, fatEntry_size);
		memcpy(&fatEntry,buffer,fatEntry_size);
		if(fatEntry == 0)
		{
			freeCount ++;
		}
		seek(fd,fatEntry_size,SEEK_CUR);
		entRead ++;
	}
	return freeCount;
}

//----------------------------------------------------------------------
// printInfo()
//----------------------------------------------------------------------
// Prints out information about the FAT32 volume
//
//----------------------------------------------------------------------
void printInfo()
{
	
	printf("Drive name: %3s", "");
	int i;
	for(i=0; i<BS_VolLab_LENGTH; i++)
	{
		printf("%c", bpb.BS_VolLab[i]);
	}
	printf("\n");
	
	//free space(kB) = # free clusters * sectors/clus * bytes/sec
	uint32_t freeClusters = countFreeClus();
	uint32_t freeSectors_bytes = freeClusters * sectors_PER_clus * bytes_PER_sec;
	printf("Free Space:%9d kB\n", freeSectors_bytes/1024);

	//amount of uable storage on volume = # sectors*bytes/sec - (BPB,FATS,RES. SIG SECT)
	int storage = (bpb.BPB_TotSec32 - (rsvd_sectors)) * bytes_PER_sec;
	printf("Usable Space:  %d kB\n", storage/1024);

	//cluster size in sectors and bytes
	printf("Cluster Size: ");
	printf(" %d Sector(s)\n", sectors_PER_clus);
	printf("\t%10d Bytes\n",bytes_PER_sec*sectors_PER_clus);
	printf("\n");
}

//----------------------------------------------------------------------
// list()
//----------------------------------------------------------------------
// Lists all the directory entries in FAT32 volume
//
// PARAMETERS:
// 		uint32_t cluster:		cluster containing the directory entries being 
//								listed
// 		int level:				The distance from the root directory
//----------------------------------------------------------------------
void list(uint32_t cluster, int level)
{
	fat32DirEntry curr_dir;
	uint32_t sector = find_sector(cluster);
	uint32_t offset = sector*bytes_PER_sec; 

	unsigned char buffer[sizeof(fat32DirEntry)];
	seek(fd, offset, SEEK_SET);
	readfile(fd, buffer, sizeof(fat32DirEntry));
	memmove(&curr_dir, &buffer, sizeof(fat32DirEntry));
	
	int entry_num = 1;
	while(curr_dir.DIR_Name[0] != 0x00 && entry_num <= dirEntry_PER_clus)
	{	
		if(curr_dir.DIR_Name[0] != 0xE5)
		{		
			if(curr_dir.DIR_Attr == 0x08)
			{
				printf("VOLUME ID - %s\n\n", curr_dir.DIR_Name);
			}
			else if(curr_dir.DIR_Attr == 0x10)
			{
				if(validCHAR(curr_dir.DIR_Name[0]) && (curr_dir.DIR_Name[0] != 0x2E))
				{
					printlevel(level);
					getname(curr_dir.DIR_Name, 0);
					printf("DIRECTORY: %s\n",dirname);
					list(curr_dir.DIR_FstClusLO, level+1);
					printf("\n");
				}	
			}
			else
			{
				if(validCHAR(curr_dir.DIR_Name[0]) && curr_dir.DIR_Attr != 0x0F)
				{
					printlevel(level);
					getname(curr_dir.DIR_Name, 1);
					printf("FILE:  %s\n", dirname);
				}
			}
		}

		//get next directory entry
		seek(fd,offset + (sizeof(fat32DirEntry)*entry_num),SEEK_SET);
		readfile(fd, buffer, sizeof(fat32DirEntry));
		memmove(&curr_dir, &buffer, sizeof(fat32DirEntry));
		entry_num ++;
	}

	//check if more clusters are assigned to the directory
	uint32_t nextCluster = checkFAT(cluster);
	if(nextCluster != 0)
	{
		list(nextCluster, level);
	}
}

//----------------------------------------------------------------------
// getFile()
//----------------------------------------------------------------------
// Searches given directory for the sub-directory/file named 'path'
//
// PARAMETERS:
// 		char *filename:		the file path for the desired file
//----------------------------------------------------------------------
void getFile(char *filename)
{
	char *delim = "/";
	char *filepath[10];
	char *name = strtok(filename, delim);
	endOS = 0;
	int count = 0;		// length of path
	if(name == NULL)
	{
		printf("Error: Empty file path\n");
	}

	while(name != NULL && count < 10)
	{
		char *upper = name;
		while(*upper)
		{
			*upper = toupper(*upper);
			upper ++;
		}
		filepath[count] = name;
		count ++;
		name = strtok(NULL,delim);
	}


	if(count == 1)
	{
		printf("Error: '%s' is a directory can not be written\n", filepath[0]);
		exit(1);
	}

	int i = 0;
	uint32_t clus = bpb.BPB_RootClus;
	char *file = NULL;
	while(i < count && !endOS)
	{
		clus = findDirectory(clus, filepath[i]);
		if(clus == 0)
		{
			printf("Error: No file/directory named '%s'\n", filepath[i]);
			exit(1);
		}
		else
		{
			file = filepath[i];
		}

		i++;
	}

	if(i != count)
	{
		printf("Error: No file/directory named '%s'\n", filepath[i]);
		exit(1);
	}

	// make new file
	new_file = open(file,O_WRONLY|O_APPEND|O_CREAT|O_EXCL);
	if(new_file < 0)
	{
		printf("Failed to create file '%s'\n", file);
		exit(1);
	}

	writeFile(clus);
	printf("File '%s' was succesful\n", file);

}

//----------------------------------------------------------------------
// writeFile()
//----------------------------------------------------------------------
// Writes cluster contents to the file being created
//
// PARAMETERS:
// 		int32_t cluster:		data cluster for file being created
//----------------------------------------------------------------------
void writeFile(int32_t cluster)
{
	uint32_t sector = find_sector(cluster);
	uint32_t offset = sector*bytes_PER_sec;
	unsigned char buffer [bytes_PER_sec];

	seek(fd, offset, SEEK_SET);
	readfile(fd, buffer, bytes_PER_sec);

	writef(new_file, buffer, bytes_PER_sec);
	uint32_t next_cluster = checkFAT(cluster);
	if(next_cluster != 0)
	{
		writeFile(next_cluster);
	}
}

//----------------------------------------------------------------------
// findDirectory()
//----------------------------------------------------------------------
// Searches given directory for the sub-directory/file named 'path'
//
// PARAMETERS:
// 		uint32_t folder:		Current directory 
// 		char* path:				The sub-directory/file name being looked for
// RETURN:
//		uint32_t nextFolder:	The cluster number of the next sub-directory 
//								or the cluster number of the start of a file
//								returns 0 if no sub-directory/file found
//----------------------------------------------------------------------
uint32_t findDirectory(uint32_t folder, char* path)
{
	fat32DirEntry curr_dir;
	uint32_t sector = find_sector(folder);
	uint32_t offset = sector*bytes_PER_sec; 

	//load in directory cluster
	unsigned char buffer[sizeof(fat32DirEntry)];
	seek(fd, offset, SEEK_SET);
	readfile(fd, buffer, sizeof(fat32DirEntry));
	memmove(&curr_dir, &buffer, sizeof(fat32DirEntry));

	uint32_t nextFolder = 0; 
	int found = 0;
	int entry_num = 1;
	

	while(!found && curr_dir.DIR_Name[0] != 0x00 && entry_num <= dirEntry_PER_clus)
	{
		if(curr_dir.DIR_Name[0] != 0xE5 && curr_dir.DIR_Attr != 0x08)
		{	
			if(curr_dir.DIR_Attr == 0x10)
			{
				if(validCHAR(curr_dir.DIR_Name[0]) && (curr_dir.DIR_Name[0] != 0x2E))
				{
					getname(curr_dir.DIR_Name, 0);
					if(compare(path) == 0)
					{
						nextFolder = curr_dir.DIR_FstClusLO;
						found = 1;
					}
				}	
			}

			else
			{
				if(validCHAR(curr_dir.DIR_Name[0]) && curr_dir.DIR_Attr != 0x0F)
				{
					getname(curr_dir.DIR_Name, 1);
					if(compare(path) == 0)
					{
						nextFolder = curr_dir.DIR_FstClusLO;
						found = 1;
						endOS = 1;
					}
				}
			}
		}

		//get next directory entry
		seek(fd,offset + (sizeof(fat32DirEntry)*entry_num),SEEK_SET);
		readfile(fd, buffer, sizeof(fat32DirEntry));
		memmove(&curr_dir, &buffer, sizeof(fat32DirEntry));
		entry_num ++;
	}

	//check if more clusters are assigned to the directory
	if(!found)
	{
		uint32_t nextCluster = checkFAT(folder);
		if(nextCluster != 0)
		{
			nextFolder = findDirectory(nextCluster, path);
		}
	}
	return nextFolder;
}

//----------------------------------------------------------------------
// checkFAT()
//----------------------------------------------------------------------
// Checks the File Allocation Table and returns the FAT entry
//
// PARAMETERS:
// 		uint32_t cluster_num:		index of the FAT to check
// RETURN:
//		uint32_t next_cluster:		next cluster after the given cluster number
//									return  0 if end of FAT chain 
//----------------------------------------------------------------------
uint32_t checkFAT(uint32_t cluster_num)
{
	uint32_t fatEntry_size = 4;	//number of bytes in a fat entry

	unsigned char buffer[fatEntry_size];
	uint32_t next_cluster;
	uint32_t offset = (filetable_start*bytes_PER_sec) + ((cluster_num)*(fatEntry_size));

	seek(fd,offset,SEEK_SET);
	readfile(fd, buffer, fatEntry_size);
	memcpy(&next_cluster,buffer,fatEntry_size);

	if(next_cluster < 0xFFFFFF8)
	{
		next_cluster = 0x0FFFFFFF & next_cluster;
	}
	else
	{
		next_cluster = 0;
	}

	return next_cluster;
}

//----------------------------------------------------------------------
// find_sector()
//----------------------------------------------------------------------
// Takes a cluster number and returns the sector in the data region
//
// PARAMETERS:
// 		uint32_t cluster_num:		Cluster number
// RETURN:
//		uint16_t sector:			Sector corresponding to the cluster number
//----------------------------------------------------------------------
uint32_t find_sector(uint32_t cluster_num)
{
	uint16_t sector = (cluster_num-2)*sectors_PER_clus;
	sector = data_region + sector;
	return sector;
}

//----------------------------------------------------------------------
// getname()
//----------------------------------------------------------------------
// Builds directory/file name without padding and dot extentions 
// for files
// Stores the completed string in dirname[]
//
// PARAMETERS:
// 		unsigned char entry[]:	DIR_Name[] from directroy entry
// 		int type:				0 if directroy 
//								1 if a file
//----------------------------------------------------------------------
void getname(unsigned char entry[], int type)
{
	int entry_index = 0;
	int i = 0;
	unsigned char name[13];
	while(entry_index < 11)
	{
		if(entry[entry_index] != ' ' && validCHAR(entry[entry_index]))
		{
			name[i] = entry[entry_index];
			i++;
		}
		entry_index++;
	}
	
	if(type == 1)
	{
		int exten_start = i-3;
		char temp1 = name[exten_start];
		name[exten_start] = '.';
		exten_start++;
		while(exten_start<i+1)
		{
			char temp2 = name[exten_start];
			name[exten_start] = temp1;
			temp1 = temp2;
			exten_start++;
		}
		i++;
	}
	
	while(i<13)
	{
		name[i] = '\0';
		i++;
	}
	memcpy(&dirname,&name,12);
}

//----------------------------------------------------------------------
// validCHAR()
//----------------------------------------------------------------------
// Checks if char from directory name is a valid char 
//
// PARAMETERS:
// 		unsigned char c:	character to check validity 
//----------------------------------------------------------------------
int validCHAR(unsigned char c)
{
	int valid = 1;
	if(c < 0x20)
	{
		valid = 0;
	}

	if(c==0x22||c==0x2A||c==0x2B||c==0x2C||c==0x2E||c==0x2F||c==0x3A||c==0x3B||
		c==0x3C||c==0x3D||c==0x3E||c==0x3F||c==0x5B||c==0x5C||c==0x5D||c==0x7C)
	{
		valid = 0;
	}
	return valid;
}

//----------------------------------------------------------------------
// printlevel()
//----------------------------------------------------------------------
// Prints indentation depending the path length from root directory
//
// PARAMETERS:
// 		int level:		the path length from root
//----------------------------------------------------------------------
void printlevel(int level)
{
	int i;

	for(i=0;i<level;i++)
	{
		printf("\t");
	}
}

//----------------------------------------------------------------------
// compare()
//----------------------------------------------------------------------
// Compares the given string with the current directory/file name
//
// PARAMETERS:
// 		char* str:		The file name being compared to current file/dir name
// RETURN:
//		int match:		returns 0 if directory names match
//						returns 1 if directory names do not match
//----------------------------------------------------------------------
int compare(char* str)
{
	int match = 0;
	unsigned int i = 0;

	while(!match && dirname[i] != '\0')
	{
		if(dirname[i] != str[i])
		{
			match = 1;
		} 

		i++;
	}

	if(i < strlen(str))
	{
		match = 1;
	}

	return match;
}


/**********************************************
	WRAPPER FUNCTIONS 
**********************************************/
int readfile(int fd, unsigned char buffer[], int size)
{
	int rc = read(fd, buffer, size);
	if(rc < 0)
	{
		printf("Failed to read disk\n");
		exit(1);
	}
	return rc;
}

int writef(int fd, unsigned char buffer[], int size)
{
	int rc = write(fd, buffer, size);
	if(rc < 0)
	{
		printf("Failed to read disk\n");
		exit(1);
	}
	return rc;
}

int seek(int fd, int offset, int whence)
{	
	int rc = lseek(fd,offset,whence);
	if(rc < 0)
	{
		printf("file seeking failed.\n");
		exit(1);
	}
	return rc;
}