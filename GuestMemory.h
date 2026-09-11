// GuestMemory.h
#pragma once
#include <cstdint>
#include <unordered_map>

#include "MemoryStructs.h"

class GuestMemory {
public:
	MemoryStatus mapPage(uint64_t pageBase, Protection protection);
	MemoryStatus unmapPage(uint64_t pageBase);
	MemoryStatus ProtectPage(uint64_t address, Protection protection);
	
	MemoryResult read(uint64_t address, void* dst, size_t size) const; 
	MemoryResult write(uint64_t address, const void* src, size_t size); 

	const Page* findPage(uint64_t address) const;

private:
	MemoryStatus validateRange(uint64_t address, size_t size, AccessType access) const;
	std::unordered_map<uint64_t, Page> pages_;
};




