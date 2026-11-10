#include "MemoryManager.h"
namespace emu {
    MemoryStatus MemoryManager::reserve(ptr_t preferredAddress, size_t size, RegionType type, PageProtection protection, uint64_t* baseOut) {

        if (size == 0) {
            return MemoryStatus::InvalidSize;
        }

        auto alignedSizeOpt = alignUp(size, PAGE_SIZE);
        if (!alignedSizeOpt) {
            return MemoryStatus::AddressOverflow;
        }

        size = *alignedSizeOpt;
        ptr_t address;

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

        ptr_t end = address + size;

        for (const auto& [_, region] : regions_) {

            ptr_t regionEnd = region.base + region.size;

            if (address < regionEnd && region.base < end) {
                return MemoryStatus::AlreadyReserved;
            }
        }

        regions_.emplace(
            address,
            MemoryRegion{
                .base = address,
                .size = size,
                .type = type,
                .allocationProtection = protection
            }
        );
        if (baseOut != nullptr) {
            *baseOut = address;
        }

        return MemoryStatus::Success;
    }

    MemoryStatus MemoryManager::commit(ptr_t address, size_t size, PageProtection protection) {

        if (size == 0) {
            return (MemoryStatus::Success);
        }

        if (size > UINT64_MAX - address) {
            return (MemoryStatus::AddressOverflow);
        }

        ptr_t requestedEnd = address + size;
        const MemoryRegion* region = findRegion(address);

        if (!region) {
            return (MemoryStatus::NotReserved);
        }

        ptr_t regionEnd = region->size + region->base;

        if (requestedEnd > regionEnd) {
            return (MemoryStatus::RangeCrossesRegion);
        }

        ptr_t commitBase = alignDown(address, PAGE_SIZE);
        auto commitEndOpt = alignUp(requestedEnd, PAGE_SIZE);

        if (!commitEndOpt) {
            return (MemoryStatus::AddressOverflow);
        }

        ptr_t commitEnd = *commitEndOpt;

        for (ptr_t page = commitBase; page < commitEnd; page += PAGE_SIZE) {

            // Page is already committed 
            if (memory_.findPage(page)) {
                continue;
            }

            MemoryStatus status = memory_.mapPage(page, protection);

            if (status != MemoryStatus::Success) {
                return (status);
            }
        }

        return MemoryStatus::Success;
    }

    MemoryStatus MemoryManager::decommit(ptr_t address, size_t size) {

        if (size == 0) {
            return (MemoryStatus::Success);
        }

        if (size > UINT64_MAX - address) {
            return (MemoryStatus::AddressOverflow);
        }

        ptr_t requestedEnd = address + size;
        const MemoryRegion* region = findRegion(address);

        if (!region) {
            return (MemoryStatus::NotReserved);
        }

        ptr_t regionEnd = region->size + region->base;

        if (requestedEnd > regionEnd) {
            return (MemoryStatus::RangeCrossesRegion);
        }

        ptr_t decommitBase = alignDown(address, PAGE_SIZE);
        auto decommitEndOpt = alignUp(requestedEnd, PAGE_SIZE);
        if (!decommitEndOpt) {
            return MemoryStatus::AddressOverflow;
        }

        ptr_t decommitEnd = *decommitEndOpt;

        for (ptr_t page = decommitBase; page < decommitEnd; page += PAGE_SIZE) {

            // Pages in the region that are reserved but not committed
            if (!memory_.findPage(page)) {
                continue;
            }

            MemoryStatus status = memory_.unmapPage(page);

            if (status != MemoryStatus::Success) {
                return (status);
            }
        }

        return MemoryStatus::Success;
    }

    MemoryStatus MemoryManager::release(ptr_t address) {
        auto it = regions_.find(address);

        if (it == regions_.end()) {
            return (MemoryStatus::InvalidRelease);
        }

        const MemoryRegion& region = it->second;
        ptr_t regionEnd = region.base + region.size;

        for (ptr_t page = region.base; page < regionEnd; page += PAGE_SIZE) {
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

    MemoryStatus MemoryManager::allocate(ptr_t preferredAddress, size_t size, PageProtection protection, RegionType type, uint64_t* baseOut) {

        ptr_t base = 0;
        MemoryStatus status = reserve(preferredAddress, size, type, protection, &base);

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

    const MemoryRegion* MemoryManager::findRegion(ptr_t address) const {
        if (regions_.empty()) {
            return nullptr;
        }

        auto it = regions_.upper_bound(address);

        if (it == regions_.begin()) {
            return nullptr;
        }

        --it;

        const MemoryRegion& region = it->second;
        ptr_t regionEnd = region.base + region.size;

        if (address >= region.base && address < regionEnd) {
            return &region;
        }

        return nullptr;
    }

    std::optional<MemoryInfo> MemoryManager::query(ptr_t address) const {

        MemoryInfo info{};

        ptr_t pageBase = alignDown(address, PAGE_SIZE);
        if (pageBase >= MAX_USER_ADDRESS_EXCLUSIVE) {
            return std::nullopt;
        }

        const MemoryRegion* region = findRegion(address);

        // region is free, just need to validate the upper address bound
        if (!region) {

            auto next = regions_.lower_bound(pageBase);

            ptr_t freeEnd = next != regions_.end()
                ? next->second.base
                : MAX_USER_ADDRESS_EXCLUSIVE;

            info.baseAddress = pageBase;
            info.regionSize = freeEnd - pageBase;
            info.state = MemoryState::Free;

            return info;
        }

        info.baseAddress = pageBase;
        info.allocationBase = region->base;
        info.allocationProtection = region->allocationProtection;
        info.type = region->type;

        const Page* page = memory_.findPage(pageBase);

        info.state = page
            ? MemoryState::Committed
            : MemoryState::Reserved;

        info.protection = page
            ? page->protection
            : PageProtection::NoAccess;

        ptr_t regionEnd = region->base + region->size;
        ptr_t current = pageBase;

        while (current < regionEnd) {
            const Page* nextPage = memory_.findPage(current);

            MemoryState nextState = nextPage
                ? MemoryState::Committed
                : MemoryState::Reserved;

            PageProtection nextProtection = nextPage
                ? nextPage->protection
                : PageProtection::NoAccess;

            if (nextState != info.state || nextProtection != info.protection) {
                break;
            }

            current += PAGE_SIZE;
        }

        info.regionSize = current - pageBase;

        return info;
    }


    std::optional<ptr_t> MemoryManager::findFreeAddress(size_t size) const {
        auto candidateOpt = alignUp(DYNAMIC_BASE, ALLOCATION_GRANULARITY);
        if (!candidateOpt) {
            return std::nullopt;
        }

        ptr_t candidate = *candidateOpt;

        for (const auto& [base, region] : regions_) {

            if (size > UINT64_MAX - candidate) {
                return std::nullopt;
            }

            ptr_t candidateEnd = candidate + size;

            if (candidateEnd <= region.base)
                return candidate;

            if (region.size > UINT64_MAX - region.base)
                return std::nullopt;

            ptr_t regionEnd = region.base + region.size;

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

    MemoryStatus MemoryManager::read(ptr_t address, void* dst, size_t size) const {
        return memory_.read(address, dst, size);
    }

    MemoryStatus MemoryManager::write(ptr_t address, const void* src, size_t size) {
        return memory_.write(address, src, size);
    }

    MemoryStatus MemoryManager::protect(ptr_t address, size_t size, PageProtection newProtect, PageProtection* oldProtect) {

        const MemoryRegion* region = findRegion(address);
        if (!region) {
            return MemoryStatus::NotReserved;
        }

        if (size > UINT64_MAX - address) {
            return MemoryStatus::AddressOverflow;
        }

        ptr_t requestedEnd = address + size;
        ptr_t regionEnd = region->base + region->size;

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

        ptr_t start = alignDown(address, PAGE_SIZE);

        // Verify that all pages in requested range are committed
        for (ptr_t page = start; page < end; page += PAGE_SIZE) {
            if (!memory_.findPage(page)) {
                return MemoryStatus::NotCommitted;
            }
        }

        PageProtection currentProtection = memory_.findPage(start)->protection;

        for (ptr_t page = start; page < end; page += PAGE_SIZE) {
            MemoryStatus status = memory_.ProtectPage(page, newProtect);

            if (status != MemoryStatus::Success) {
                return status;
            }
        }

        if (oldProtect) {
            *oldProtect = currentProtection;
        }

        return MemoryStatus::Success;
    }

    MemoryStatus MemoryManager::readString(ptr_t address, std::string& out, size_t maxLength) const {
        out.clear();

        for (size_t i = 0; i < maxLength; ++i) {
            char c;

            MemoryStatus status = read(
                address + i,
                &c,
                sizeof(c)
            );

            if (status != MemoryStatus::Success) {
                return status;
            }

            if (c == '\0') {
                return MemoryStatus::Success;
            }

            out.push_back(c);
        }

        return MemoryStatus::InvalidSize;
    }

};