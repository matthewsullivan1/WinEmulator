// GuestMemory.h
#pragma once
#include <unordered_map>

#include "MemoryStructs.h"

namespace emu {
	class GuestMemory {
	public:
		MemoryStatus mapPage(ptr_t pageBase, PageProtection protection);
		MemoryStatus unmapPage(ptr_t pageBase);
		MemoryStatus ProtectPage(ptr_t address, PageProtection protection);

		MemoryStatus read(ptr_t address, void* dst, size_t size) const;
		MemoryStatus write(ptr_t address, const void* src, size_t size);

		const Page* findPage(ptr_t address) const;

	private:
		MemoryStatus validateRange(ptr_t address, size_t size, AccessType access) const;
		std::unordered_map<ptr_t, Page> pages_;
	};
};



