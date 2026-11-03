#pragma once
#include <cstdint>

struct _GUEST_PEB {
	uint8_t                          Reserved1[2];
	uint8_t                          BeingDebugged;
	uint8_t                          Reserved2[1];
	uint64_t                         Reserved3[1];
	uint64_t						 ImageBaseAddress;					// Offset is 0x10
	uint64_t						 Ldr;								
	uint64_t						 ProcessParameters = 0;					// Need guest compatible structure eventually for more complex programs
	uint64_t                         Reserved4[3];
	uint64_t                         AtlThunkSListPtr;
	uint64_t                         Reserved5;
	uint32_t                         Reserved6;
	uint64_t                         Reserved7;
	uint32_t                         Reserved8;
	uint32_t                         AtlThunkSListPtr32;
	uint64_t                         Reserved9[45];
	uint8_t                          Reserved10[96];
	uint64_t						 PostProcessInitRoutine;				
	uint8_t                          Reserved11[128];
	uint64_t                         Reserved12[1];
	uint32_t                         SessionId = 0;

};

struct _GUEST_LIST_ENTRY {
	uint64_t Flink;
	uint64_t Blink;
};

struct _GUEST_UNICODE_STRING {
	uint16_t Length;
	uint16_t MaximumLength;
	uint32_t Padding;
	uint64_t Buffer;
};

struct _GUEST_PEB_LDR_DATA {
	uint32_t Length;
	uint8_t Initialized;
	uint8_t Padding[3];
	uint64_t SsHandle;

	_GUEST_LIST_ENTRY InLoadOrderModuleList;
	_GUEST_LIST_ENTRY InMemoryOrderModuleList;
	_GUEST_LIST_ENTRY InInitializationOrderModuleList;
};

struct _GUEST_LDR_DATA_TABLE_ENTRY {
	_GUEST_LIST_ENTRY InLoadOrderLinks;
	_GUEST_LIST_ENTRY InMemoryOrderLinks;
	_GUEST_LIST_ENTRY InInitializationOrderLinks;
	uint64_t DllBase;
	uint64_t EntryPoint;
	uint32_t SizeOfImage;
	uint32_t Padding;
	_GUEST_UNICODE_STRING FullDllName;
	_GUEST_UNICODE_STRING BaseDllName;
};