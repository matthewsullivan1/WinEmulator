#include "MemoryManager.h"


std::expected<uint64_t, MemoryStatus> MemoryManager::reserve(uint64_t preferredAddress, size_t size, RegionType type) {

    if (size == 0) {
        return std::unexpected(MemoryStatus::InvalidSize);
    }

    auto alignedSizeOpt = alignUp(size, PAGE_SIZE);
    if (!alignedSizeOpt) {
        return std::unexpected(MemoryStatus::AddressOverflow);
    }

    size = *alignedSizeOpt;
    uint64_t address;

    if (preferredAddress == 0) {
        
        auto addressOpt = findFreeAddress(size);
        
        if (!addressOpt) {
            return std::unexpected<MemoryStatus>(MemoryStatus::AddressOverflow); // Need to update with more informative error since this case prob wont happen often
        }

        address = *addressOpt;
    }
    else {

        if ((preferredAddress % ALLOCATION_GRANULARITY) != 0) {
            return std::unexpected(MemoryStatus::MisalignedAddress);
        }

        if (size > UINT64_MAX - preferredAddress) {
            return std::unexpected<MemoryStatus>(MemoryStatus::AddressOverflow);
        }

        address = preferredAddress;
    }

    uint64_t end = address + size;

    for (const auto& [base, region] : regions_) {

        uint64_t regionEnd = region.base + region.size;

        if (address < regionEnd && region.base < end) {
            return std::unexpected(MemoryStatus::AlreadyReserved);
        }
    }

    regions_.emplace(
        address,
        MemoryRegion{
            .base = address,
            .size = size,
            .type = type
        }
    );

    return address; 
}

std::expected<uint64_t, MemoryStatus> MemoryManager::commit(uint64_t address, size_t size, Protection protection) {
    
    if (size == 0) {
        return std::unexpected(MemoryStatus::InvalidSize);
    }

    if (size > UINT64_MAX - address) {
        return std::unexpected(MemoryStatus::AddressOverflow);
    }
    
    uint64_t requestedEnd = address + size;
    const MemoryRegion* region = findRegion(address);

    if (!region) {
        return std::unexpected(MemoryStatus::NotReserved);
    }

    uint64_t regionEnd = region->size + region->base;

    if (requestedEnd > regionEnd) {
        return std::unexpected(MemoryStatus::RangeCrossesRegion);
    }

    uint64_t commitBase = alignDown(address, PAGE_SIZE);
    auto commitEndOpt = alignUp(requestedEnd, PAGE_SIZE);

    if (!commitEndOpt) {
        return std::unexpected(MemoryStatus::AddressOverflow);
    }

    uint64_t commitEnd = *commitEndOpt;

    for (uint64_t page = commitBase; page < commitEnd; page += PAGE_SIZE) {
        if (memory_.findPage(page)) {
            return std::unexpected(MemoryStatus::AlreadyCommitted);
        }
    }

    for (uint64_t page = commitBase; page < commitEnd; page += PAGE_SIZE) {
        MemoryStatus status = memory_.mapPage(page, protection);

        if (status != MemoryStatus::Success) {
            return std::unexpected(status);
        }
    }

    return commitBase;
}

std::expected<void, MemoryStatus> MemoryManager::decommit(uint64_t address, size_t size) {

    if (size == 0) {
        return std::unexpected(MemoryStatus::InvalidSize);
    }

    if (size > UINT64_MAX - address) {
        return std::unexpected(MemoryStatus::AddressOverflow);
    }

    uint64_t requestedEnd = address + size;
    const MemoryRegion* region = findRegion(address);

    if (!region) {
        return std::unexpected(MemoryStatus::NotReserved);
    }

    uint64_t regionEnd = region->size + region->base;

    if (requestedEnd > regionEnd) {
        return std::unexpected(MemoryStatus::RangeCrossesRegion);
    }

    uint64_t decommitBase = alignDown(address, PAGE_SIZE);

    for (uint64_t page = decommitBase; page < requestedEnd; page += PAGE_SIZE) {
        if (!memory_.findPage(page)) {
            return std::unexpected(MemoryStatus::NotCommitted);
        }
    }

    for (uint64_t page = decommitBase; page < requestedEnd; page += PAGE_SIZE) {
        MemoryStatus status = memory_.unmapPage(page);

        if (status != MemoryStatus::Success) {
            return std::unexpected(status);
        }
    }

    return {};
}

std::expected<void, MemoryStatus> MemoryManager::release(uint64_t address) {
    auto it = regions_.find(address);

    if (it == regions_.end()) {
        return std::unexpected(MemoryStatus::InvalidRelease);
    }

    const MemoryRegion& region = it->second;
    uint64_t regionEnd = region.base + region.size;

    for (uint64_t page = region.base; page < regionEnd; page += PAGE_SIZE) {
        if (memory_.findPage(page)) {
            MemoryStatus status = memory_.unmapPage(page);

            if (status != MemoryStatus::Success) {
                return std::unexpected(status);
            }
        }
    }

    regions_.erase(it);

    return {};
}


std::optional<uint64_t> MemoryManager::findFreeAddress(size_t size) const {
	auto candidateOpt = alignUp(DYNAMIC_BASE, ALLOCATION_GRANULARITY);
	if (!candidateOpt) {
		return std::nullopt;
	}

	uint64_t candidate = *candidateOpt;

	for (const auto& [base, region] : regions_) {

		if (size> UINT64_MAX - candidate) {
			return std::nullopt;
		}
		
        uint64_t candidateEnd = candidate + size;

        if (candidateEnd <= region.base)
            return candidate;

        if (region.size > UINT64_MAX - region.base)
            return std::nullopt;

        uint64_t regionEnd = region.base + region.size;

        if (candidate < regionEnd) {
            auto nextOpt = alignUp(regionEnd, ALLOCATION_GRANULARITY);

            if (!nextOpt) {
                return std::nullopt;
            }

            candidate = *nextOpt;
        }
	}

    if (size > UINT64_MAX - candidate) {
        return std::nullopt;
    }

    return candidate;
}