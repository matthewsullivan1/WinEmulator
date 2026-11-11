#include "PEFile.h"
#include <fstream>
#include <iostream>
#include <string>
#include <algorithm>

using win::IMAGE_DOS_HEADER;
using win::IMAGE_OPTIONAL_HEADER;
using win::IMAGE_NT_HEADERS;
using win::IMAGE_FILE_HEADER;
using win::IMAGE_DATA_DIRECTORY;
using win::IMAGE_SECTION_HEADER;

using win::WORD;
using win::DWORD;
using win::ULONGLONG;

std::expected<PEFile, PEError> PEFile::open(const std::filesystem::path& path) {
	PEFile pe;
	pe.path_ = path;

	std::ifstream file(path, std::ios::binary | std::ios::ate);

	if (!file) {
		return std::unexpected(PEError::FileOpenFailed);
	}

	auto end = file.tellg();

	if (end < 0) {
		return std::unexpected(PEError::FileReadFailed);
	}

	if (end == 0) {
		return std::unexpected(PEError::EmptyFile);
	}

	if (static_cast<std::uintmax_t>(end) > std::numeric_limits<size_t>::max()) {
		return std::unexpected(PEError::FileReadFailed);
	}

	pe.data_.resize(static_cast<size_t>(end));

	file.seekg(0, std::ios::beg);

	if (!file.read(reinterpret_cast<char*>(pe.data_.data()), static_cast<std::streamsize>(pe.data_.size()))) {
		return std::unexpected(PEError::FileReadFailed);
	}

	auto result = pe.parseHeaders();

	if (!result) {
		return std::unexpected(result.error());
	}

	return pe;
}

//*********************************************************************************************************//
// Accessors
//*********************************************************************************************************//

template<typename T> std::optional<T> PEFile::view(size_t offset) const {

	static_assert(std::is_trivially_copyable_v<T>);


	if (offset > data_.size() || sizeof(T) > data_.size() - offset) {
		return std::nullopt;
	}

	T result;
	std::memcpy(&result, data_.data() + offset, sizeof(T));

	return result;
}

IMAGE_DOS_HEADER PEFile::dosHeader() const {
	return *view<IMAGE_DOS_HEADER>(0);
}

IMAGE_NT_HEADERS PEFile::ntHeaders() const {
	return *view<IMAGE_NT_HEADERS>(ntOffset_);
}

std::optional<IMAGE_SECTION_HEADER> PEFile::sectionHeader(size_t index) const {
	
	if (index >= numberOfSections_) {
		return std::nullopt;
	}

	return view<IMAGE_SECTION_HEADER>(sectionTableOffset_ + index * sizeof(IMAGE_SECTION_HEADER));
}

std::span<const std::uint8_t> PEFile::data() const { 
	return data_; 
}

//*********************************************************************************************************//
// Validation
//*********************************************************************************************************//

std::expected<void, PEError> PEFile::parseHeaders() {

	auto result = validateDosHeader();
	if (!result) {
		return result;
	}

	result = validateNtHeaders();
	if (!result) {
		return result;
	}

	result = validateSections();
	if (!result) {
		return result;
	}

	return {};
}

std::expected<void, PEError> PEFile::validateDosHeader() {

	auto dos = view<IMAGE_DOS_HEADER>(0);
	
	if (!dos) {
		return std::unexpected(PEError::InvalidDOSHeader);
	}

	if (dos->e_magic != win::IMAGE_DOS_SIGNATURE) {
		return std::unexpected(PEError::InvalidDOSHeader);
	}

	if (dos->e_lfanew < 0) {
		return std::unexpected(PEError::InvalidDOSHeader);
	}

	ntOffset_ = static_cast<size_t>(dos->e_lfanew);
	
	return {};
}

std::expected<void, PEError> PEFile::validateNtHeaders() {

	// NT Header validation
	auto nt = view<win::IMAGE_NT_HEADERS>(ntOffset_);

	if (!nt) {
		return std::unexpected(PEError::InvalidNTHeader);
	}

	if (nt->Signature != win::IMAGE_NT_SIGNATURE) {
		return std::unexpected(PEError::InvalidNTHeader);
	}

	// File Header validation
	auto& file = nt->FileHeader;

	if (file.Machine != win::IMAGE_FILE_MACHINE_AMD64) {
		return std::unexpected(PEError::UnsupportedArchitecture);
	}

	if (file.NumberOfSections == 0) {
		return std::unexpected(PEError::InvalidNTHeader);
	}

	if (file.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER)) {
		return std::unexpected(PEError::InvalidOptionalHeader);
	}

	// Optional Header validation
	auto& opt = nt->OptionalHeader; 

	if (opt.Magic != win::IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
		return std::unexpected(PEError::InvalidOptionalHeader);
	}

	if (opt.SectionAlignment == 0 ||
		opt.FileAlignment == 0 ||
		opt.SectionAlignment < opt.FileAlignment ||
		!std::has_single_bit(opt.SectionAlignment) ||
		!std::has_single_bit(opt.FileAlignment)) {

		return std::unexpected(PEError::InvalidOptionalHeader);
	}

	// Special case where section alignment is below 1kb
	if (opt.SectionAlignment < 0x1000 && opt.SectionAlignment != opt.FileAlignment) {
		return std::unexpected(PEError::InvalidOptionalHeader);
	}

	if (opt.SizeOfImage == 0 ||
		opt.SizeOfHeaders == 0 ||
		opt.SizeOfHeaders > data_.size()) {
		
		return std::unexpected(PEError::InvalidOptionalHeader);
	}

	if (opt.NumberOfRvaAndSizes > win::IMAGE_NUMBEROF_DIRECTORY_ENTRIES) {
		return std::unexpected(PEError::InvalidOptionalHeader);
	}

	if (opt.AddressOfEntryPoint >= opt.SizeOfImage && opt.AddressOfEntryPoint != 0) {
		return std::unexpected(PEError::InvalidOptionalHeader);
	}

	size_t optionalOffset = ntOffset_ + offsetof(IMAGE_NT_HEADERS, OptionalHeader);

	if (optionalOffset > data_.size() || file.SizeOfOptionalHeader > data_.size() - optionalOffset) {
		return std::unexpected(PEError::InvalidOptionalHeader);
	}

	sectionTableOffset_ = optionalOffset + file.SizeOfOptionalHeader;
	size_t sectionTableSize = static_cast<size_t>(file.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);

	if (sectionTableSize > data_.size() - sectionTableOffset_ ||
		sectionTableSize > opt.SizeOfHeaders ||
		sectionTableOffset_ > opt.SizeOfHeaders - sectionTableSize) {
		
		return std::unexpected(PEError::InvalidSectionTable);
	}

	return {};
}

std::expected<void, PEError> PEFile::validateSections() {
	
	const auto nt = view<IMAGE_NT_HEADERS>(ntOffset_);

	if (!nt) {
		return std::unexpected(PEError::InvalidNTHeader);
	}

	const auto& opt = nt->OptionalHeader;

	uint64_t previousEnd = opt.SizeOfHeaders;

	for (size_t i = 0; i < nt->FileHeader.NumberOfSections; i++) {

		size_t offset = sectionTableOffset_ + (i * sizeof(IMAGE_SECTION_HEADER));

		auto section = view<IMAGE_SECTION_HEADER>(offset);

		if (!section) {
			return std::unexpected(PEError::InvalidSectionTable);
		}

		if (section->SizeOfRawData != 0) {
			if (section->PointerToRawData > data_.size() || section->SizeOfRawData > data_.size() - section->PointerToRawData) {
				return std::unexpected(PEError::InvalidSectionTable);
			}

			if (section->PointerToRawData % opt.FileAlignment != 0) {
				return std::unexpected(PEError::InvalidSectionTable);
			}
		}

		if (section->VirtualAddress % opt.SectionAlignment != 0) {
			return std::unexpected(PEError::InvalidSectionTable);
		}

		uint64_t mappedSize = std::max(static_cast<uint64_t>(section->Misc.VirtualSize), static_cast<uint64_t>(section->SizeOfRawData));
		uint64_t sectionEnd = static_cast<uint64_t>(section->VirtualAddress) + mappedSize;

		if (sectionEnd > opt.SizeOfImage) {
			return std::unexpected(PEError::InvalidSectionTable);
		}

		if (section->VirtualAddress < previousEnd) {
			return std::unexpected(PEError::InvalidSectionTable);
		}

		uint64_t alignment = opt.SectionAlignment;
		previousEnd = ((sectionEnd + alignment - 1) / alignment) * alignment;

	}

	numberOfSections_ = nt->FileHeader.NumberOfSections;

	return {};
}
