// GuestMemory.cpp 
#include "GuestMemory.h"

namespace emu {

	MemoryStatus GuestMemory::mapPage(ptr_t pageBase, PageProtection protection) {

		if (pageBase % PAGE_SIZE != 0) {
			return MemoryStatus::MisalignedAddress;
		}

		if (pages_.contains(pageBase)) {
			return MemoryStatus::AddressMapped;
		}

		pages_.emplace(pageBase, Page{
			.data = {},
			.protection = protection
			});

		return MemoryStatus::Success;
	}

	MemoryStatus GuestMemory::unmapPage(ptr_t pageBase) {
		if (pageBase % PAGE_SIZE != 0) {
			return MemoryStatus::MisalignedAddress;
		}

		if (!(pages_.contains(pageBase))) {
			return MemoryStatus::InvalidAddress;
		}

		pages_.erase(pageBase);

		return MemoryStatus::Success;
	}

	MemoryStatus GuestMemory::ProtectPage(ptr_t pageBase, PageProtection protection) {
		if (pageBase % PAGE_SIZE != 0) {
			return MemoryStatus::MisalignedAddress;
		}

		if (!(pages_.contains(pageBase))) {
			return MemoryStatus::InvalidAddress;
		}

		pages_.at(pageBase).protection = protection;

		return MemoryStatus::Success;
	}

	MemoryStatus GuestMemory::read(ptr_t address, void* dst, size_t size) const {
		if (!dst) {
			return MemoryStatus::InvalidAddress;
		}

		MemoryStatus status = validateRange(address, size, AccessType::Read);
		if (status != MemoryStatus::Success) {
			return status;
		}

		auto* out = static_cast<uint8_t*>(dst);

		size_t remaining = size;
		ptr_t currentAddr = address;

		while (remaining > 0) {
			ptr_t pageBase = alignDown(currentAddr, PAGE_SIZE);
			size_t offset = currentAddr - pageBase;

			const Page& page = pages_.at(pageBase);
			size_t bytesToCopy = std::min(remaining, PAGE_SIZE - offset);

			std::memcpy(out, page.data.data() + offset, bytesToCopy);

			out += bytesToCopy;
			currentAddr += bytesToCopy;
			remaining -= bytesToCopy;
		}

		return MemoryStatus::Success;
	}

	MemoryStatus GuestMemory::write(ptr_t address, const void* src, size_t size) {
		if (!src) {
			return MemoryStatus::InvalidAddress;
		}

		MemoryStatus status = validateRange(address, size, AccessType::Write);
		if (status != MemoryStatus::Success) {
			return status;
		}

		const auto* in = static_cast<const uint8_t*>(src);

		size_t remaining = size;
		ptr_t currentAddr = address;



		while (remaining > 0) {
			ptr_t pageBase = alignDown(currentAddr, PAGE_SIZE);
			size_t offset = currentAddr - pageBase;

			Page& page = pages_.at(pageBase);
			size_t bytesToCopy = std::min(remaining, PAGE_SIZE - offset);

			memcpy(page.data.data() + offset, in, bytesToCopy);

			in += bytesToCopy;
			currentAddr += bytesToCopy;
			remaining -= bytesToCopy;
		}

		return MemoryStatus::Success;
	}

	MemoryStatus GuestMemory::validateRange(ptr_t address, size_t size, AccessType access) const {
		if (size == 0) {
			return MemoryStatus::Success;
		}

		if (size > UINT64_MAX - address) {
			return MemoryStatus::AddressOverflow;
		}

		ptr_t currentAddr = address;
		size_t remaining = size;

		while (remaining > 0) {
			ptr_t pageBase = alignDown(currentAddr, PAGE_SIZE);
			size_t offset = currentAddr - pageBase;

			auto it = pages_.find(pageBase);

			if (it == pages_.end()) {
				return MemoryStatus::InvalidAddress;
			}

			switch (access) {
			case AccessType::Read:
				if (!canRead(it->second.protection))
					return MemoryStatus::AccessViolation;
				break;

			case AccessType::Write:
				if (!canWrite(it->second.protection))
					return MemoryStatus::AccessViolation;
				break;

			case AccessType::Execute:
				if (!canExecute(it->second.protection))
					return MemoryStatus::AccessViolation;
				break;
			}

			size_t chunk = std::min(remaining, PAGE_SIZE - offset);

			currentAddr += chunk;
			remaining -= chunk;
		}

		return MemoryStatus::Success;
	}

	const Page* GuestMemory::findPage(ptr_t address) const {
		ptr_t pageBase = alignDown(address, PAGE_SIZE);

		auto it = pages_.find(pageBase);

		if (it == pages_.end()) {
			return nullptr;
		}

		return &it->second;
	}

};