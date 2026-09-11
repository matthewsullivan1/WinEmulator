#pragma once
#include <array>
#include <optional>

constexpr uint64_t PAGE_SIZE = 0x1000;
constexpr uint64_t ALLOCATION_GRANULARITY = 0x10000;
constexpr uint64_t DYNAMIC_BASE = 0x0000000010000000;

constexpr uint64_t alignDown(uint64_t value, uint64_t alignment) {
    return value & ~(alignment - 1);
}

inline std::optional<uint64_t> alignUp(uint64_t value, uint64_t alignment) {
    if (alignment == 0) {
        return std::nullopt;
    }

    if (value > UINT64_MAX - (alignment - 1)) {
        return std::nullopt;
    }

    return (value + alignment - 1) & ~(alignment - 1);
}

enum class RegionType {
    Private,
    Image,
    Mapped
};

enum class PageState {
    Reserved,
    Committed
};

enum class AccessType {
    Read,
    Write,
    Execute
};

enum class MemoryStatus {
    Success,

    InvalidSize,
    InvalidAddress,
    AddressOverflow,
    MisalignedAddress,
    NoFreeAddress,

    AddressMapped,
    AlreadyReserved,
    AlreadyCommitted,

    NotMapped,
    NotReserved,
    NotCommitted,

    RangeCrossesRegion,

    ProtectionViolation,
    RegionNotFound,
    InvalidRelease
};

struct Protection {
    bool r = false;
    bool w = false;
    bool x = false;
};

struct Page {
    std::array<uint8_t, PAGE_SIZE> data{};
    Protection protection{};
};

struct PageChunk {
    uint64_t pageBase;
    size_t offset;
    size_t size;
};

struct MemoryRegion {
    uint64_t base = 0;
    size_t size = 0;
    RegionType type = RegionType::Private;
};

struct MemoryInfo {
    uint64_t regionBase;
    uint64_t pageBase;
    size_t regionSize;

    PageState state;
    Protection protection;
    RegionType type;
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
