// MemoryManager.h
#pragma once
#include <cstdint>
#include <map>
#include <array>
#include <expected>
#include <optional>
#include <string>

#include "MemoryStructs.h"
#include "GuestMemory.h"

class MemoryManager {
public:
	explicit MemoryManager(GuestMemory& memory) : memory_(memory) {}

	MemoryStatus reserve(
		uint64_t preferredAddress, 
		size_t size, 
		RegionType type,
		uint64_t* baseOut = nullptr);

	MemoryStatus commit(
		uint64_t address, 
		size_t size, 
		Protection protection);

	MemoryStatus decommit(
		uint64_t address, 
		size_t size);

	MemoryStatus release(
		uint64_t address);

	MemoryStatus allocate(
		uint64_t preferredAddress, 
		size_t size, 
		Protection protection, 
		RegionType type,
		uint64_t* baseOut = nullptr);

	std::optional<MemoryInfo> query(uint64_t address) const;

	MemoryStatus read(uint64_t address, void* dst, size_t size) const;
	MemoryStatus write(uint64_t address, const void* src, size_t size);
	MemoryStatus protect(uint64_t address, size_t size, Protection protection);

	MemoryStatus readString(
		uint64_t address,
		std::string& out,
		size_t maxLen
	) const;

private:
	const MemoryRegion* findRegion(uint64_t address) const;
	std::optional<uint64_t> findFreeAddress(size_t size) const;

	GuestMemory& memory_;
	std::map<uint64_t, MemoryRegion> regions_;
};

// Reservation : 64KiB address aligned (0x10000), of any size
// Any operation should operate under the bounds of one reservation 
// Commit	   : Page align requested address down, page align size up, create (base + size) / PAGE_SIZE pages 
// Allocate    : Combines reserve + commit 