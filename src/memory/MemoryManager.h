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
#include "common/types.h"


namespace emu {
	class MemoryManager {
	public:
		explicit MemoryManager(GuestMemory& memory) : memory_(memory) {}

		MemoryStatus reserve(
			ptr_t preferredAddress,
			size_t size,
			RegionType type,
			PageProtection protection,
			uint64_t* baseOut = nullptr);

		MemoryStatus commit(
			ptr_t address,
			size_t size,
			PageProtection protection);

		MemoryStatus decommit(
			ptr_t address,
			size_t size);

		MemoryStatus release(
			ptr_t address);

		MemoryStatus allocate(
			ptr_t preferredAddress,
			size_t size,
			PageProtection protection,
			RegionType type,
			uint64_t* baseOut = nullptr);

		std::optional<MemoryInfo> query(ptr_t address) const;

		MemoryStatus read(
			ptr_t address,
			void* dst,
			size_t size) const;

		MemoryStatus write(
			ptr_t address,
			const void* src,
			size_t size);

		MemoryStatus protect(
			ptr_t address,
			size_t size,
			PageProtection newProtect,
			PageProtection* oldProtect = nullptr);

		MemoryStatus readString(
			ptr_t address,
			std::string& out,
			size_t maxLen
		) const;

	private:
		const MemoryRegion* findRegion(ptr_t address) const;
		std::optional<ptr_t> findFreeAddress(size_t size) const;

		GuestMemory& memory_;
		std::map<ptr_t, MemoryRegion> regions_;
	};
};
// Reservation : 64KiB address aligned (0x10000), of any size
// Any operation should operate under the bounds of one reservation 
// Commit	   : Page align requested address down, page align size up, create (base + size) / PAGE_SIZE pages 
// Allocate    : Combines reserve + commit 