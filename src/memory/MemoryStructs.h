#pragma once
#include <array>
#include <optional>
#include "MemoryConst.h"
#include "common/types.h"

using emu::ptr_t;

struct Page {
    std::array<uint8_t, PAGE_SIZE> data{};
    PageProtection protection = PageProtection::NoAccess;
};

struct PageChunk {
    ptr_t pageBase;
    size_t offset;
    size_t size;
};

struct MemoryRegion {
    ptr_t base = 0;
    size_t size = 0;
    RegionType type = RegionType::Private;
    PageProtection allocationProtect = PageProtection::NoAccess;
};

struct MemoryInfo {
    ptr_t baseAddress = 0;
    ptr_t allocationBase = 0;
    size_t regionSize = 0;

    MemoryState state = MemoryState::Free;
    PageProtection protection = PageProtection::NoAccess;
    PageProtection allocationProtection = PageProtection::NoAccess;
    RegionType type = RegionType::Private;
};

struct MemoryResult {
    MemoryStatus status;
    size_t bytesTransferred = 0;

    bool succeeded() const {
        return status == MemoryStatus::Success;
    }
};

inline PageChunk getPageChunk(uint64_t address, size_t remaining) {
    uint64_t pageBase = alignDown(address, PAGE_SIZE);
    size_t offset = address & (PAGE_SIZE - 1);

    return {
        .pageBase = pageBase,
        .offset = offset,
        .size = (((remaining) < (PAGE_SIZE - offset)) ? (remaining) : (PAGE_SIZE - offset))
    };
}
