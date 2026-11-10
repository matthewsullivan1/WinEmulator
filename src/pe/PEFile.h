#pragma once

#include <cstdint>
#include <vector>
#include <span>
#include <string>
#include <filesystem>

#include "windows/PeStructs.h"

// Update to either utilities to parse a PE or a lightweight class. Needs to use wintypes.h typedefs

class PEFile {
public:
	explicit PEFile(const std::filesystem::path& path);

	std::size_t fileSize() const;
	template<typename T> const T* view(std::size_t offset) const;

	const win::IMAGE_DOS_HEADER* dosHeader()				const; //return pointer to dos header
	const win::IMAGE_NT_HEADERS64* ntHeaders()			const; // return pointer to nt header
	const win::IMAGE_FILE_HEADER* fileHeader()			const; // return pointer to file header
	const win::IMAGE_OPTIONAL_HEADER* optionalHeader()	const; // return pointer to optional header
	const win::IMAGE_DATA_DIRECTORY* dataDirectorty()	const; // return pointer to data directory base address
	const win::IMAGE_SECTION_HEADER* sectionHeader()		const; // return pointer to section header base address
	std::span<const IMAGE_SECTION_HEADER> sections() const; // return pointer to section header as well as number of sections
	const win::DWORD entryPointRva() const;
	const win::ULONGLONG imageBase() const;
	const win::IMAGE_SECTION_HEADER* sectionFromRva(win::DWORD rva) const;
	const std::optional<size_t> rvaToFileOffset(win::DWORD rva) const;
	std::span<const std::uint8_t> data() const;

	bool parseHeaders();
	void print_bytes(const void* data, std::size_t size);
	template<typename T> void printField(int indent, const char* type, const char* name, T value) const;
	void printStruct(int indent, const char* type, const char* name) const;

	void summary()					const;
	void displayNtHeaders()			const;
	void displayFileHeader()		const;
	void displayOptionalHeader()	const;
	void displayDataDirectory()		const;
	void displaySectionHeaders()	const; 

private:
	std::vector<uint8_t> data_;		// Contains raw bytes of PE
	std::size_t size_;				// Size of raw PE
	const std::filesystem::path& path_;
	
	bool validateDos()				const; // validate DOS header (e_magic)
	bool validateNt()				const; // validate NT headers (signature)
	bool validateFileHeader()		const;
	bool validateOptionalHeader()	const; // validates 64 bit PE (magic)
	
};