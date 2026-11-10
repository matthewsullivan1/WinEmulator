#pragma once
#include <cstdint>
#include "NtStructs.h"

using emu::ptr_t;

using win::BYTE;
using win::DWORD;
using win::PVOID;

using win::LIST_ENTRY;
using win::UNICODE_STRING;


struct PEB {
	BYTE							Reserved1[2];
	BYTE							BeingDebugged;
	BYTE							Reserved2[1];
	uint64_t						Reserved3[1];
	ptr_t							ImageBaseAddress;
	ptr_t							Ldr;
	ptr_t							ProcessParameters = 0;				
	uint64_t                        Reserved4[3];
	uint64_t                        AtlThunkSListPtr;
	uint64_t                        Reserved5;
	DWORD							Reserved6;
	uint64_t                        Reserved7;
	DWORD							Reserved8;
	DWORD							AtlThunkSListPtr32;
	uint64_t                        Reserved9[45];
	BYTE							Reserved10[96];
	ptr_t							PostProcessInitRoutine;				
	BYTE							Reserved11[128];
	uint64_t						Reserved12[1];
	DWORD							SessionId = 0;

};

struct PEB_LDR_DATA {
	uint32_t Length;
	uint8_t Initialized;
	uint8_t Padding[3];
	uint64_t SsHandle;

	LIST_ENTRY InLoadOrderModuleList;
	LIST_ENTRY InMemoryOrderModuleList;
	LIST_ENTRY InInitializationOrderModuleList;
};

struct LDR_DATA_TABLE_ENTRY {
	LIST_ENTRY InLoadOrderLinks;
	LIST_ENTRY InMemoryOrderLinks;
	LIST_ENTRY InInitializationOrderLinks;
	uint64_t DllBase;
	uint64_t EntryPoint;
	uint32_t SizeOfImage;
	uint32_t Padding;
	UNICODE_STRING FullDllName;
	UNICODE_STRING BaseDllName;
};

struct RTL_USER_PROCESS_PARAMETERS {
	BYTE Reserved1[16];
	PVOID 
};