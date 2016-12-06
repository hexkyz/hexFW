#ifndef FSA_H
#define FSA_H

#define FSA_MOUNTFLAGS_BINDMOUNT 	(1 << 0)
#define FSA_MOUNTFLAGS_GLOBAL 		(1 << 1)

typedef struct
{
	u32 unk[0x19];
	char name[0x100];
} directoryEntry_s;

typedef struct
{
	u32 unk1[0x4];
	u32 size; 		// size in bytes
	u32 physsize; 	// physical size on disk in bytes
	u32 unk2[0x13];
} fileStat_s;

int fsaInit();
int fsaExit();

int FSA_Open();

int FSA_Mount(char* device_path, char* volume_path, u32 flags, char* arg_string, int arg_string_len);
int FSA_Unmount(char* path, u32 flags);

int FSA_GetDeviceInfo(char* device_path, int type, u32* out_data);

int FSA_MakeDir(char* path, u32 flags);
int FSA_OpenDir(char* path, int* outHandle);
int FSA_ReadDir(int handle, directoryEntry_s* out_data);
int FSA_CloseDir(int handle);

int FSA_OpenFile(char* path, char* mode, int* outHandle);
int FSA_ReadFile(void* data, u32 size, u32 cnt, int fileHandle, u32 flags);
int FSA_WriteFile(void* data, u32 size, u32 cnt, int fileHandle, u32 flags);
int FSA_StatFile(int handle, fileStat_s* out_data);
int FSA_CloseFile(int handle);

int FSA_RawOpen(char* device_path, int* outHandle);
int FSA_RawRead(void* data, u32 size_bytes, u32 cnt, u64 sector_offset, int device_handle);
int FSA_RawWrite(void* data, u32 size_bytes, u32 cnt, u64 sector_offset, int device_handle);
int FSA_RawClose(int device_handle);

#endif