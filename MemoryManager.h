// MemoryManager.h
#pragma once
#include <cstdint>
#include <map>
#include <array>
#include <expected>
#include <optional>

#include "MemoryStructs.h"
#include "GuestMemory.h"

class MemoryManager {
public:
	explicit MemoryManager(GuestMemory& memory) : memory_(memory) {}

	std::expected<uint64_t, MemoryStatus> reserve(
		uint64_t preferredAddress, 
		size_t size, 
		RegionType type);

	std::expected<uint64_t, MemoryStatus> commit(
		uint64_t address, 
		size_t size, 
		Protection protection);

	std::expected<void, MemoryStatus> decommit(
		uint64_t address, 
		size_t size);

	std::expected<void, MemoryStatus> release(
		uint64_t address);

	std::expected<uint64_t, MemoryStatus> allocate(
		uint64_t preferredAddress, 
		size_t size, 
		Protection protection, 
		RegionType type);

	const MemoryRegion* findRegion(uint64_t address) const;

	std::optional<MemoryInfo> query(uint64_t address) const;

private:
	std::optional<uint64_t> findFreeAddress(size_t size) const;

	GuestMemory& memory_;
	std::map<uint64_t, MemoryRegion> regions_;
};

// Reservation : 64KiB address aligned (0x10000), of any size
// Commit	   : Pages within the reservation address range should now actually exist in the regions GuestMemory instance
// Allocate    : Combines reserve + commit 