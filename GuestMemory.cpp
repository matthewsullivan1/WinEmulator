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

// Operates on entire MemoryRegion. Address must be regions_ key, and size must be equal to regions_[address].size
bool GuestMemory::unmap(uint64_t address, size_t size) {
	if (size == 0) {
		return false;
	}

	// Note that any value [1, 4096] will pass this check
	if (address % PAGE_SIZE != 0) {
		return false;
	}

	size_t alignedSize = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	// Either address is incorrect or is not mapped
	if (!(regions_.contains(address))) {
		return false;
	}

	// Size is invalid
	if (regions_.at(address).size != alignedSize) {
		return false;
	}

	for (uint64_t current = address; current < address + alignedSize; current += PAGE_SIZE) {
		pages_.erase(current);
	}

	regions_.erase(address);
	return true;
}

bool GuestMemory::protect(uint64_t address, size_t size, Protection protection) {
	if (size == 0) {
		return false;
	}

	// Note that any value [1, 4096] will pass this check
	if (address % PAGE_SIZE != 0) {
		return false;
	}

	size_t alignedSize = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	// Either address is incorrect or is not mapped
	if (!(regions_.contains(address))) {
		return false;
	}

	// Size is invalid
	if (regions_.at(address).size != alignedSize) {
		return false;
	}

	for (uint64_t current = address; current < address + alignedSize; current += PAGE_SIZE) {
		pages_.at(current).protection = protection;
	}

	regions_.at(address).protection = protection;

	return true;
}

bool GuestMemory::read(uint64_t address, void* dst, size_t size) const {
	if (size == 0) {
		return true;
	}

	auto* out = static_cast<uint8_t*>(dst);
	size_t remaining = size;
	uint64_t currentAddress = address; 

	while (remaining > 0) {
		// Mask offset
		uint64_t pageBase = currentAddress & ~(PAGE_SIZE - 1);
		// Mask pageBase
		size_t offset = currentAddress & (PAGE_SIZE - 1);

		auto it = pages_.find(pageBase);

		if (it == pages_.end()) {
			return false;
		}

		const Page& page = it->second;

		if (!page.protection.r) {
			return false;
		}

		size_t availableInPage = PAGE_SIZE - offset;
		size_t bytesToCopy = (((remaining) < (availableInPage)) ? (remaining) : (availableInPage));
		std::memcpy(
			out,
			page.data.data() + offset,
			bytesToCopy
		);
		
		out += bytesToCopy;
		currentAddress += bytesToCopy;
		remaining -= bytesToCopy;
	}

	return true;
}

bool GuestMemory::write(uint64_t dst, void* src, size_t size) {
	if (size == 0) {
		return true;
	}




}


