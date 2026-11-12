#pragma once
#include <cstdint>
#include "NtStructs.h"

namespace win {
	struct PEB {
		BYTE							Reserved1[2];
		BYTE							BeingDebugged;
		BYTE							Reserved2[1];
		uint64_t						Reserved3[1];
		uint64_t							ImageBaseAddress;
		uint64_t							Ldr;
		uint64_t							ProcessParameters = 0;
		uint64_t                        Reserved4[3];
		uint64_t                        AtlThunkSListPtr;
		uint64_t                        Reserved5;
		DWORD							Reserved6;
		uint64_t                        Reserved7;
		DWORD							Reserved8;
		DWORD							AtlThunkSListPtr32;
		uint64_t                        Reserved9[45];
		BYTE							Reserved10[96];
		uint64_t							PostProcessInitRoutine;
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

	struct API_SET_NAMESPACE {
		ULONG Version;
		ULONG Size;
		ULONG Flags;
		ULONG Count;
		ULONG EntryOffset;
		ULONG HashOffset;
		ULONG HashFactor;
	};

	struct API_SET_NAMESPACE_ENTRY {
		ULONG Flags;
		ULONG NameOffset;
		ULONG NameLength;
		ULONG HashedLength;
		ULONG ValueOffset;
		ULONG ValueCount;
	};

	struct API_SET_HASH_ENTRY {
		ULONG Flags;
		ULONG NameOffset;
		ULONG NameLength;
		ULONG HashedLength;
		ULONG ValueOffset;
		ULONG ValueCount;
	};

	struct API_SET_VALUE_ENTRY {
		ULONG Flags;
		ULONG NameOffset;
		ULONG NameLength;
		ULONG ValueOffset;
		ULONG ValueLength;
	};

};