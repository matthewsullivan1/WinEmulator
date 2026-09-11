#include <cstdint>
#include <unordered_map>
#include <iostream>
#include <Windows.h>

#include "GuestMemory.h"
#include "MemoryStructs.h"

MemoryStatus GuestMemory::mapPage(uint64_t pageBase, Protection protection) {
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

MemoryStatus GuestMemory::unmapPage(uint64_t pageBase) {
	if (pageBase % PAGE_SIZE != 0) {
		return MemoryStatus::MisalignedAddress;
	}

	if (!(pages_.contains(pageBase))) {
		return MemoryStatus::InvalidAddress;
	}

	pages_.erase(pageBase);

	return MemoryStatus::Success;
}

MemoryStatus GuestMemory::ProtectPage(uint64_t pageBase, Protection protection) {
	if (pageBase % PAGE_SIZE != 0) {
		return MemoryStatus::MisalignedAddress;
	}

	if (!(pages_.contains(pageBase))) {
		return MemoryStatus::InvalidAddress;
	}

	pages_.at(pageBase).protection = protection;

	return MemoryStatus::Success;
}

MemoryResult GuestMemory::read(uint64_t address, void* dst, size_t size) const {
	if (!dst) {
		return MemoryResult{ .status = MemoryStatus::InvalidAddress };
	}
	
	MemoryStatus status = validateRange(address, size, AccessType::Read);
	if (status != MemoryStatus::Success) {
		return MemoryResult{ .status = status };
	}

	auto* out = static_cast<uint8_t*>(dst);

	size_t remaining = size;
	uint64_t currentAddr = address;

	MemoryResult m{};

	while (remaining > 0) {
		uint64_t pageBase = alignDown(currentAddr, PAGE_SIZE);
		size_t offset = currentAddr - pageBase;

		const Page& page = pages_.at(pageBase);
		size_t bytesToCopy = std::min(remaining, PAGE_SIZE - offset);

		std::memcpy(out, page.data.data() + offset, bytesToCopy);

		m.bytesTransferred += bytesToCopy;
		out += bytesToCopy;
		currentAddr += bytesToCopy;
		remaining -= bytesToCopy;
	}

	m.status = MemoryStatus::Success;
	return m;
}

MemoryResult GuestMemory::write(uint64_t address, const void* src, size_t size) {
	if (!src) {
		return MemoryResult{ .status = MemoryStatus::InvalidAddress };
	}

	MemoryStatus status = validateRange(address, size, AccessType::Write);
	if (status != MemoryStatus::Success) {
		return MemoryResult{ .status = status };
	}

	const auto* in = static_cast<const uint8_t*>(src);

	size_t remaining = size;
	uint64_t currentAddr = address;

	MemoryResult m{};

	while (remaining > 0) {
		uint64_t pageBase = alignDown(currentAddr, PAGE_SIZE);
		size_t offset = currentAddr - pageBase;

		Page& page = pages_.at(pageBase);
		size_t bytesToCopy = std::min(remaining, PAGE_SIZE - offset);
		
		memcpy(page.data.data() + offset, in, bytesToCopy);

		m.bytesTransferred += bytesToCopy;
		in += bytesToCopy;
		currentAddr += bytesToCopy;
		remaining -= bytesToCopy;
	}

	m.status = MemoryStatus::Success;
	return m;
}

MemoryStatus GuestMemory::validateRange(uint64_t address, size_t size, AccessType access) const {
	if (size == 0) {
		return MemoryStatus::Success;
	}

	if (size > UINT64_MAX - address) {
		return MemoryStatus::AddressOverflow;
	}

	uint64_t currentAddr = address;
	size_t remaining = size;

	while (remaining > 0) {
		uint64_t pageBase = alignDown(currentAddr, PAGE_SIZE);
		size_t offset = currentAddr - pageBase;

		auto it = pages_.find(pageBase);

		if (it == pages_.end()) {
			return MemoryStatus::InvalidAddress;
		}

		switch (access) {
		case AccessType::Read:
			if (!it->second.protection.r)
				return MemoryStatus::ProtectionViolation;
			break;

		case AccessType::Write:
			if (!it->second.protection.w)
				return MemoryStatus::ProtectionViolation;
			break;

		case AccessType::Execute:
			if (!it->second.protection.x)
				return MemoryStatus::ProtectionViolation;
			break;
		}

		size_t chunk = std::min(remaining, PAGE_SIZE - offset);

		currentAddr += chunk;
		remaining -= chunk;
	}

	return MemoryStatus::Success;
}

const Page* GuestMemory::findPage(uint64_t address) const {
	uint64_t pageBase = alignDown(address, PAGE_SIZE);

	auto it = pages_.find(pageBase);

	if (it == pages_.end()) {
		return nullptr;
	}

	return &it->second;
}
