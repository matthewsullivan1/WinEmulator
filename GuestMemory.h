#pragma once
#include <cstdint>
#include <unordered_map>
#include <array>

constexpr uint64_t PAGE_SIZE = 0x1000;

struct Protection {
	bool r = false;
	bool w = false;
	bool x = false;
};

struct MemoryRegion {
	uint64_t base = 0;
	size_t size = 0;
	Protection protection{};
};

struct Page {
	std::array<uint8_t, PAGE_SIZE> data{};
	Protection protection{};
};

class GuestMemory {
public:
	bool map(uint64_t address, size_t size, Protection protection);
	bool unmap(uint64_t address, size_t size);
	bool protect(uint64_t address, size_t size, Protection protection);

	bool read(uint64_t address, void* dst, size_t size) const; 
	bool write(uint64_t address, void* src, size_t size); 

private:
	std::unordered_map<uint64_t, Page> pages_;
	std::unordered_map<uint64_t, MemoryRegion> regions_;
};