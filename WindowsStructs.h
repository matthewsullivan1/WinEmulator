#pragma once
#include <cstdint>

struct _GuestPEB {
	uint8_t                          Reserved1[2];
	uint8_t                          BeingDebugged;
	uint8_t                          Reserved2[1];
	uint64_t                         Reserved3[2];
	uint64_t						 Ldr;								// Need to define PBR_LDR_DATA, LIST_ENTRY
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