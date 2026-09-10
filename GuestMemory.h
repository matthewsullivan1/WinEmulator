#pragma once
#include <cstdint>
#include <unordered_map>
#include <array>
#include <optional>

/*****************************************************************************/
// Defs
/*****************************************************************************/

constexpr uint64_t PAGE_SIZE = 0x1000;
constexpr uint64_t ALLOCATION_GRANULARITY = 0x10000;
constexpr uint64_t DYNAMIC_BASE = 0x0000010000000000;

constexpr uint64_t alignDown(uint64_t value, uint64_t alignment) {
	return value & ~(alignment - 1);
}

constexpr uint64_t alignUp(uint64_t value, uint64_t alignment) {
	return (value + alignment - 1) & ~(alignment - 1);
}

/*****************************************************************************/
// GuestMemory
/*****************************************************************************/

class GuestMemory {
public:
	std::optional<uint64_t> reserve(uint64_t preferredAddress, size_t size, RegionType type);
	bool commit(uint64_t address, size_t size, Protection protection);
	bool decommit(uint64_t address, size_t size);
	bool release(uint64_t address, size_t size);
	bool protect(uint64_t address, size_t size, Protection protection);

	bool map(uint64_t address, size_t size, Protection protection);
	bool unmap(uint64_t address, size_t size);
	
	bool read(uint64_t address, void* dst, size_t size) const; 
	bool write(uint64_t address, void* src, size_t size); 

	std::optional<uint64_t> allocate(uint64_t preferredAddress, size_t size, Protection protection, RegionType type);

	const MemoryRegion* findRegion(uint64_t address) const;
	const Page* findPage(uint64_t address) const;
	std::optional<MemoryInfo> query(uint64_t address) const;

private:
	std::unordered_map<uint64_t, Page> pages_;
	std::unordered_map<uint64_t, MemoryRegion> regions_;
};


/*****************************************************************************/
// Structs
/*****************************************************************************/
struct Protection {
	bool r = false;
	bool w = false;
	bool x = false;
};

struct MemoryRegion {
	uint64_t base = 0;
	size_t size = 0;

	RegionType type;
};

struct Page {
	std::unique_ptr<std::array<uint8_t, PAGE_SIZE>> data;

	PageState state = PageState::Reserved;
	Protection protection{};
};

struct MemoryInfo {
	uint64_t regionBase;
	uint64_t pageBase;
	size_t regionSize;
	PageState state;
	Protection protection;
	RegionType type;
};

/*****************************************************************************/
// Enums
/*****************************************************************************/

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

enum class MemoryError {
	None,
	InvalidSize,
	InvalidAddress,
	AddressOverflow,
	MisalignedAddress,
	AlreadyReserved,
	NotReserved,
	NotCommitted,
	ProtectionViolation,
	RegionNotFound,
	InvalidRelease
};




