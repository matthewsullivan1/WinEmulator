#include <cstdint>
#include <unordered_map>
#include <iostream>
#include <Windows.h>

#include "GuestMemory.h"

bool GuestMemory::map(uint64_t address, size_t size, Protection protection) {
	if (size == 0) {
		return false;
	}
	
	if (address % PAGE_SIZE != 0) {
		return false;
	}

	size_t alignedSize = (size + PAGE_SIZE - 1) & !(PAGE_SIZE - 1);

	for (uint64_t current = address; current < address + alignedSize; current += PAGE_SIZE) {
		if (pages_.find(current) != pages_.end()) {
			return false;
		}
	}

	MemoryRegion region{
		.base = address,
		.size = alignedSize,
		.protection = protection
	};

	regions_.emplace(address, region);

	for (uint64_t current = address; current < address + alignedSize; current += PAGE_SIZE) {
		Page page; 
		page.protection = protection; 
		pages_.emplace(current, std::move(page));
	}

	return true;
}
