#include "MemoryManager.h"


MemoryStatus MemoryManager::reserve(uint64_t preferredAddress, size_t size, RegionType type, uint64_t* baseOut) {

    if (size == 0) {
        return MemoryStatus::InvalidSize;
    }

    auto alignedSizeOpt = alignUp(size, PAGE_SIZE);
    if (!alignedSizeOpt) {
        return MemoryStatus::AddressOverflow;
    }

    size = *alignedSizeOpt;
    uint64_t address;

    if (preferredAddress == 0) {
        
        auto addressOpt = findFreeAddress(size);
        
        if (!addressOpt) {
            return MemoryStatus::NoFreeAddress;
        }

        address = *addressOpt;
    }
    else {

        if ((preferredAddress % ALLOCATION_GRANULARITY) != 0) {
            return MemoryStatus::MisalignedAddress;
        }

        if (size > UINT64_MAX - preferredAddress) {
            return MemoryStatus::AddressOverflow;
        }

        address = preferredAddress;
    }

    uint64_t end = address + size;

    for (const auto& [_, region] : regions_) {

        uint64_t regionEnd = region.base + region.size;

        if (address < regionEnd && region.base < end) {
            return MemoryStatus::AlreadyReserved;
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
    if (baseOut != nullptr) {
        *baseOut = address;
    }

    return MemoryStatus::Success; 
}

MemoryStatus MemoryManager::commit(uint64_t address, size_t size, Protection protection) {
    
    if (size == 0) {
        return (MemoryStatus::Success);
    }

    if (size > UINT64_MAX - address) {
        return (MemoryStatus::AddressOverflow);
    }
    
    uint64_t requestedEnd = address + size;
    const MemoryRegion* region = findRegion(address);

    if (!region) {
        return (MemoryStatus::NotReserved);
    }

    uint64_t regionEnd = region->size + region->base;

    if (requestedEnd > regionEnd) {
        return (MemoryStatus::RangeCrossesRegion);
    }

    uint64_t commitBase = alignDown(address, PAGE_SIZE);
    auto commitEndOpt = alignUp(requestedEnd, PAGE_SIZE);

    if (!commitEndOpt) {
        return (MemoryStatus::AddressOverflow);
    }

    uint64_t commitEnd = *commitEndOpt;

    for (uint64_t page = commitBase; page < commitEnd; page += PAGE_SIZE) {
        if (memory_.findPage(page)) {
            return (MemoryStatus::AlreadyCommitted);
        }
    }

    for (uint64_t page = commitBase; page < commitEnd; page += PAGE_SIZE) {
        MemoryStatus status = memory_.mapPage(page, protection);

        if (status != MemoryStatus::Success) {
            return (status);
        }
    }

    return MemoryStatus::Success;
}

MemoryStatus MemoryManager::decommit(uint64_t address, size_t size) {

    if (size == 0) {
        return (MemoryStatus::Success);
    }

    if (size > UINT64_MAX - address) {
        return (MemoryStatus::AddressOverflow);
    }

    uint64_t requestedEnd = address + size;
    const MemoryRegion* region = findRegion(address);

    if (!region) {
        return (MemoryStatus::NotReserved);
    }

    uint64_t regionEnd = region->size + region->base;

    if (requestedEnd > regionEnd) {
        return (MemoryStatus::RangeCrossesRegion);
    }

    uint64_t decommitBase = alignDown(address, PAGE_SIZE);
    auto decommitEndOpt = alignUp(requestedEnd, PAGE_SIZE);
    if (!decommitEndOpt) {
        return MemoryStatus::AddressOverflow;
    }

    uint64_t decommitEnd = *decommitEndOpt;

    for (uint64_t page = decommitBase; page < decommitEnd; page += PAGE_SIZE) {
        if (!memory_.findPage(page)) {
            return (MemoryStatus::NotCommitted);
        }
    }

    for (uint64_t page = decommitBase; page < decommitEnd; page += PAGE_SIZE) {
        MemoryStatus status = memory_.unmapPage(page);

        if (status != MemoryStatus::Success) {
            return (status);
        }
    }

    return MemoryStatus::Success;
}

MemoryStatus MemoryManager::release(uint64_t address) {
    auto it = regions_.find(address);

    if (it == regions_.end()) {
        return (MemoryStatus::InvalidRelease);
    }

    const MemoryRegion& region = it->second;
    uint64_t regionEnd = region.base + region.size;

    for (uint64_t page = region.base; page < regionEnd; page += PAGE_SIZE) {
        if (memory_.findPage(page)) {
            MemoryStatus status = memory_.unmapPage(page);

            if (status != MemoryStatus::Success) {
                return (status);
            }
        }
    }

    regions_.erase(it);

    return MemoryStatus::Success;
}

MemoryStatus MemoryManager::allocate(uint64_t preferredAddress, size_t size, Protection protection, RegionType type, uint64_t* baseOut) {

    uint64_t base = 0;
    MemoryStatus status = reserve(preferredAddress, size, type, &base);

    if (status != MemoryStatus::Success) {
        return status;
    }

    status = commit(base, size, protection);

    if (status != MemoryStatus::Success) {
        release(base);
        return status;
    }

    if (baseOut != nullptr) {
        *baseOut = base;
        return MemoryStatus::Success;
    }

    return MemoryStatus::Success;
}

const MemoryRegion* MemoryManager::findRegion(uint64_t address) const {
    if (regions_.empty()) {
        return nullptr;
    }

    auto it = regions_.upper_bound(address);

    if (it == regions_.begin()) {
        return nullptr;
    }

    --it;

    const MemoryRegion& region = it->second;
    uint64_t regionEnd = region.base + region.size;

    if (address >= region.base && address < regionEnd) {
        return &region;
    }
    
    return nullptr;
}

std::optional<MemoryInfo> MemoryManager::query(uint64_t address) const {
    const MemoryRegion* region = findRegion(address);

    if (!region) {
        return std::nullopt;
    }

    uint64_t pageBase = alignDown(address, PAGE_SIZE);
    const Page* page = memory_.findPage(pageBase);

    MemoryInfo info = {
        .regionBase = region->base,
        .pageBase = pageBase,
        .regionSize = region->size,
        .state = page ? PageState::Committed : PageState::Reserved,
        .protection = page ? page->protection : Protection{},
        .type = region->type
    };

    return info;
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

MemoryStatus MemoryManager::read(uint64_t address, void* dst, size_t size) const {
    return memory_.read(address, dst, size);
}

MemoryStatus MemoryManager::write(uint64_t address, const void* src, size_t size) {
    return memory_.write(address, src, size);
}

MemoryStatus MemoryManager::protect(uint64_t address, size_t size, Protection protection) {

    const MemoryRegion* region = findRegion(address);
    if (!region) {
        return MemoryStatus::NotReserved;
    }

    if (size > UINT64_MAX - address) {
        return MemoryStatus::AddressOverflow;
    }
    
    uint64_t requestedEnd = address + size;
    uint64_t regionEnd = region->base + region->size;

    if (requestedEnd > regionEnd) {
        return MemoryStatus::RangeCrossesRegion;
    }

    if (size == 0) {
        return MemoryStatus::Success;
    }

    auto end = alignUp(address + size, PAGE_SIZE);
    
    if (!end) {
        return MemoryStatus::AddressOverflow;
    }

    uint64_t start = alignDown(address, PAGE_SIZE);

    // Note that this operation is not atomic. protection changes made before failure will persist
    for (uint64_t page = start; page < end; page += PAGE_SIZE) {
        MemoryStatus status = memory_.ProtectPage(page, protection);

        if (status != MemoryStatus::Success) {
            return status;
        }
    }

    return MemoryStatus::Success;

}