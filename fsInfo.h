#pragma once 

#include <inttypes.h>

#define FSI_Reserved1_LENGTH 480
#define FSI_Reserved2_LENGTH 12

#pragma pack(push)
#pragma pack(1)
struct fat32InfoSector_struct {
	uint32_t FSI_LeadSig;
	char FSI_Reserved1[FSI_Reserved1_LENGTH];
	uint32_t FSI_StructSig;
	uint32_t FSI_Free_Count; 
	uint32_t FSI_Nxt_Free;
	char FSI_Reserved2[FSI_Reserved2_LENGTH];
	uint32_t FSI_TrailSig;
};

#pragma pack(pop)

typedef struct fat32InfoSector_struct fat32FSInfo;