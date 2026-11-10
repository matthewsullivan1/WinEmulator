#pragma once

#include <cstdint>

// Fixed width windows scalar types

namespace win {
	using BYTE = std::uint8_t;
	using WORD = std::uint16_t;
	using DWORD = std::uint32_t;
	using LONG = std::int32_t;
	using ULONG = std::uint32_t;
	using WCHAR = char16_t;

	using HANDLE = std::uint64_t;
	using PVOID = std::uint64_t;
	using ULONGLONG = std::uint64_t;



}