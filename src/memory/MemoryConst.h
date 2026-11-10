#pragma once

#include <cstdint>
#include <optional>

constexpr uint64_t PAGE_SIZE = 0x1000;
constexpr uint64_t ALLOCATION_GRANULARITY = 0x10000;
constexpr uint64_t DYNAMIC_BASE = 0x0000000010000000;

enum class RegionType {
    Private,
    Image,
    Mapped
};

enum class MemoryState {
    Free,
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