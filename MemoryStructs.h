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
    AccessViolation,
    RegionNotFound,
    InvalidRelease
};

enum class PageProtection {
    NoAccess = 0x01,
    ReadOnly = 0x02,
    ReadWrite = 0x04,
    WriteCopy = 0x08,
    Execute = 0x10,
    ExecuteRead = 0x20,
    ExecuteReadWrite = 0x40,
    ExecuteWriteCopy = 0x80
};

struct Protection {
    bool r = false;
    bool w = false;
    bool x = false;
};



struct Page {
    std::array<uint8_t, PAGE_SIZE> data{};
    PageProtection protection = PageProtection::NoAccess;
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
    PageProtection protection;
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

constexpr bool canRead(PageProtection protection) noexcept {
    auto base = static_cast<PageProtection>(static_cast<uint32_t>(protection) & 0xFF);

    switch (base) {
    case PageProtection::ReadOnly:
    case PageProtection::ReadWrite:
    case PageProtection::WriteCopy:
    case PageProtection::ExecuteRead:
    case PageProtection::ExecuteReadWrite:
    case PageProtection::ExecuteWriteCopy:
        return true;

    default:
        return false;
    }
}

constexpr bool canWrite(PageProtection protection) noexcept {
    auto base = static_cast<PageProtection>(static_cast<uint32_t>(protection) & 0xFF);

    switch (base) {
    case PageProtection::ReadWrite:
    case PageProtection::WriteCopy:
    case PageProtection::ExecuteReadWrite:
    case PageProtection::ExecuteWriteCopy:
        return true;

    default:
        return false;
    }
}

constexpr bool canExecute(PageProtection protection) noexcept {
    auto base = static_cast<PageProtection>(static_cast<uint32_t>(protection) & 0xFF);

    switch (base) {
    case PageProtection::Execute:
    case PageProtection::ExecuteRead:
    case PageProtection::ExecuteReadWrite:
    case PageProtection::ExecuteWriteCopy:
        return true;

    default:
        return false;
    }
}