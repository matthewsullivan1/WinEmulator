#pragma once

#include <cstdint>
#include <vector>
#include <span>
#include <string>
#include <filesystem>
#include <expected>

#include "PeConst.h"
#include "windows/PeStructs.h"

class PEFile {
public:
	// Returns constructed PEFile only on successful validation
	static std::expected<PEFile, PEError> open(const std::filesystem::path& path);

	// Accessors
	win::IMAGE_DOS_HEADER dosHeader() const;
	win::IMAGE_NT_HEADERS ntHeaders() const;
	std::optional<win::IMAGE_SECTION_HEADER> sectionHeader(size_t index) const;
	uint16_t sections() { return numberOfSections_; }
	std::span<const std::uint8_t> data() const;

private:
	PEFile() = default;

	std::vector<uint8_t> data_;
	std::filesystem::path path_;

	size_t ntOffset_;
	size_t sectionTableOffset_; 
	uint16_t numberOfSections_;
	
	template<typename T> std::optional<T> view(std::size_t offset) const;
	std::expected<void, PEError> parseHeaders();
	std::expected<void, PEError> validateDosHeader();
	std::expected<void, PEError> validateNtHeaders();
	std::expected<void, PEError> validateSections();

};