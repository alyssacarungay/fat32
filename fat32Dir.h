#pragma once 

#include <inttypes.h>

#define DIR_Name_Length 11

#pragma pack(push)
#pragma pack(1)
struct fat32DirectoryEntry_struct {
	unsigned char DIR_Name[DIR_Name_Length];
	uint8_t DIR_Attr;
	uint8_t DIR_NTRes;
	uint8_t DIR_CrtTimeTenth;
	uint16_t DIR_CrtTime;
	uint16_t DIR_CrtDate;
	uint16_t DIR_LstAccDate;
	uint16_t DIR_FstClusHI;
	uint16_t DIR_WrtTime;
	uint16_t DIR_WrtDate;
	uint16_t DIR_FstClusLO;
	uint32_t DIR_FileSize;
};

#pragma pack(pop)

typedef struct fat32DirectoryEntry_struct fat32DirEntry;