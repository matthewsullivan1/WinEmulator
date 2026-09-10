#include "PEFile.h"
#include <fstream>
#include <iostream>
#include <string>
#include <algorithm>

PEFile::PEFile(const std::filesystem::path& path) : path_(path), size_(0) {
	if (!std::filesystem::exists(path)) {
		std::cerr << "Path " << path << " does not exist" << std::endl;
		return; 
	}

	std::ifstream file(path_, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		std::cerr << "Failed to open file " << path << std::endl;
		return;
	}

	size_ = file.tellg();
	data_.resize(size_);
	file.seekg(0);

	if (!(file.read(reinterpret_cast<char*>(this->data_.data()), this->size_))) {
		std::cerr << "Failed to read: " << path << std::endl;
		return;
	}

	file.close();

}

std::size_t PEFile::fileSize() const { return size_; }

bool PEFile::parseHeaders() { 
	const IMAGE_DOS_HEADER* dos = dosHeader();
	if (!dos || !validateDos()) {
		return false;
	}

	const IMAGE_NT_HEADERS64* nt = ntHeaders();
	if (!nt || !validateNt()) {
		return false;
	}

	const IMAGE_FILE_HEADER* file = fileHeader();
	if (!file || !validateFileHeader()) { 
		return false; 
	}

	const IMAGE_OPTIONAL_HEADER* optional = optionalHeader();
	if (!optional || !validateOptionalHeader()) {
		return false;
	}

	summary();

	return true; 
}


void PEFile::print_bytes(const void* data, std::size_t size) {

	const auto* bytes = static_cast<const unsigned char*>(data);


	for (size_t i = 0; i < size; ++i) {
		std::printf("%02X ", bytes[i]);
	}
	std::printf("\n");
}


//*********************************************************************************************************//
// Accessors
//*********************************************************************************************************//
template<typename T> const T* PEFile::view(size_t offset) const {
	// Returns address of (data_.data() + offset) cast to T*
	// Assumes [offset + sizeof(T)] is the base of a valid structure of T and is aligned properly

	if (offset + sizeof(T) > size_) { return nullptr; }

	return reinterpret_cast<const T*>(data_.data() + offset);
}

const IMAGE_DOS_HEADER* PEFile::dosHeader() const {
	return view<const IMAGE_DOS_HEADER>(0);
}

const IMAGE_NT_HEADERS64* PEFile::ntHeaders() const {

	const IMAGE_DOS_HEADER* dos = dosHeader();
	if (!dos) {
		return nullptr;
	}

	return view<const IMAGE_NT_HEADERS64>(dos->e_lfanew);
}

const IMAGE_FILE_HEADER* PEFile::fileHeader() const {
	const IMAGE_NT_HEADERS64* nt = ntHeaders();
	if (!nt) { 
		return nullptr; 
	}

	return &nt->FileHeader;
}

const IMAGE_OPTIONAL_HEADER* PEFile::optionalHeader() const {
	const IMAGE_NT_HEADERS64* nt = ntHeaders();
	if (!nt) {
		return nullptr;
	}

	return &nt->OptionalHeader;
}

const IMAGE_DATA_DIRECTORY* PEFile::dataDirectorty() const {
	const IMAGE_OPTIONAL_HEADER* optional = optionalHeader();
	if (!optional) {
		return nullptr;
	}

	return optional->DataDirectory;
}

const IMAGE_SECTION_HEADER* PEFile::sectionHeader() const {
	const IMAGE_NT_HEADERS64* nt = ntHeaders();
	if (!nt) {
		return nullptr; 
	}

	return IMAGE_FIRST_SECTION(nt);
}

std::span<const IMAGE_SECTION_HEADER> PEFile::sections() const {

	if (ntHeaders() && fileHeader()) {
		return {
			IMAGE_FIRST_SECTION(ntHeaders()),
			fileHeader()->NumberOfSections
		};
	}
	else {
		printf("Error reading section header\n");
	}
}

const DWORD PEFile::entryPointRva() const {
	return optionalHeader()->AddressOfEntryPoint;
}

const ULONGLONG PEFile::imageBase() const {
	return optionalHeader()->ImageBase;
}

const IMAGE_SECTION_HEADER* PEFile::sectionFromRva(DWORD rva) const {
	for (const auto& section : sections()) {
		const DWORD start = section.VirtualAddress;
		const DWORD end = start + max(section.Misc.VirtualSize, section.SizeOfRawData);

		if (rva >= start && rva < end) {
			return &section;
		}
	}

	return nullptr;
}

const std::optional<std::size_t> PEFile::rvaToFileOffset(DWORD rva) const {
	const auto* section = sectionFromRva(rva);
	if (!section) {
		return std::nullopt;
	}

	const auto offset = static_cast<std::size_t>(section->PointerToRawData) + (rva - section->VirtualAddress);

	if (offset >= data_.size()) {
		return std::nullopt;
	}

	return offset;
}

std::span<const uint8_t> PEFile::data() const {
	return { data_.data(), data_.size() }; 
}

//*********************************************************************************************************//
// Validation
//*********************************************************************************************************//

bool PEFile::validateDos() const {
	return (dosHeader()->e_magic == IMAGE_DOS_SIGNATURE);
}

bool PEFile::validateNt() const {
	return (ntHeaders()->Signature == IMAGE_NT_SIGNATURE);
}

bool PEFile::validateFileHeader() const {
	std::cout << "Implement validateFileHeader" << std::endl;
	return true;
}

bool PEFile::validateOptionalHeader() const {
	return (ntHeaders()->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC);
}

//*********************************************************************************************************//
// Section Display
//*********************************************************************************************************//
void PEFile::displayNtHeaders() const {
	const IMAGE_NT_HEADERS64* nt = ntHeaders();
	if (!nt) { return; }
	printStruct(0, "IMAGE_NT_HEADERS64", "NtHeaders");
	printField(4, "DWORD", "Signature", nt->Signature);
	displayFileHeader();
	displayOptionalHeader();
}

void PEFile::displayFileHeader() const {
	const IMAGE_FILE_HEADER* file = fileHeader();
	if (!file) { 
		printf("Error displaying file header\n");
		return; 
	}
	printStruct(4, "IMAGE_FILE_HEADER", "FileHeader");
	printField(8, "WORD", "Machine", file->Machine);
	printField(8, "WORD", "NumberOfSections", file->NumberOfSections);
	printField(8, "DWORD", "TimeDateStamp", file->TimeDateStamp);
	printField(8, "DWORD", "PointerToSymbolTable", file->PointerToSymbolTable);
	printField(8, "DWORD", "NumberOfSymbols", file->NumberOfSymbols);
	printField(8, "WORD", "SizeOfOptionalHeader", file->SizeOfOptionalHeader);
	printField(8, "WORD", "Characteristics", file->Characteristics);
}

void PEFile::displayOptionalHeader() const {
	const IMAGE_OPTIONAL_HEADER* optional = optionalHeader();
	if (!optional) {
		printf("Error displaying optional header\n");
		return;
	}

	printStruct(4, "IMAGE_OPTIONAL_HEADER", "OptionalHeader");
	printField(8, "WORD", "Magic", optional->Magic);
	printField(8, "BYTE", "MajorLinkerVersion", optional->MajorLinkerVersion);
	printField(8, "BYTE", "MinorLinkerVersion", optional->MinorLinkerVersion);
	printField(8, "DWORD", "SizeOfCode", optional->SizeOfCode);
	printField(8, "DWORD", "SizeOfInitializedData", optional->SizeOfInitializedData);
	printField(8, "DWORD", "SizeOfUninitializedData", optional->SizeOfUninitializedData);
	printField(8, "DWORD", "AddressOfEntryPoint", optional->AddressOfEntryPoint);
	printField(8, "DWORD", "BaseOfCode", optional->BaseOfCode);
	printField(8, "ULONGLONG", "ImageBase", optional->ImageBase);
	printField(8, "DWORD", "SectionAlignment", optional->SectionAlignment);
	printField(8, "DWORD", "FileAlignment", optional->FileAlignment);
	printField(8, "WORD", "MajorOperatingSystemVersion", optional->MajorOperatingSystemVersion);
	printField(8, "WORD", "MinorOperatingSystemVersion", optional->MinorOperatingSystemVersion);
	printField(8, "WORD", "MajorImageVersion", optional->MajorImageVersion);
	printField(8, "WORD", "MinorImageVersion", optional->MinorImageVersion);
	printField(8, "WORD", "MajorSubsystemVersion", optional->MajorSubsystemVersion);
	printField(8, "WORD", "MinorSubsystemVersion", optional->MinorSubsystemVersion);
	printField(8, "DWORD", "Win32VersionValue", optional->Win32VersionValue);
	printField(8, "DWORD", "SizeOfImage", optional->SizeOfImage);
	printField(8, "DWORD", "SizeOfHeaders", optional->SizeOfHeaders);
	printField(8, "DWORD", "CheckSum", optional->CheckSum);
	printField(8, "WORD", "Subsystem", optional->Subsystem);
	printField(8, "WORD", "DllCharacteristics", optional->DllCharacteristics);
	printField(8, "ULONGLONG", "SizeOfStackReserve", optional->SizeOfStackReserve);
	printField(8, "ULONGLONG", "SizeOfStackCommit", optional->SizeOfStackCommit);
	printField(8, "ULONGLONG", "SizeOfHeapReserve", optional->SizeOfHeapReserve);
	printField(8, "ULONGLONG", "SizeOfHeapCommit", optional->SizeOfHeapCommit);
	printField(8, "DWORD", "LoaderFlags", optional->LoaderFlags);
	printField(8, "DWORD", "NumberOfRvaAndSizes", optional->NumberOfRvaAndSizes);
	displayDataDirectory();
}

void PEFile::displayDataDirectory() const {

	const char* directoryNames[] = {
		"Export",
		"Import",
		"Resource",
		"Exception",
		"Security",
		"Base Relocation",
		"Debug",
		"Architecture",
		"Global Ptr",
		"TLS",
		"Load Config",
		"Bound Import",
		"IAT",
		"Delay Import",
		"CLR Runtime",
		"Reserved"
	};

	const IMAGE_OPTIONAL_HEADER* optional = optionalHeader();
	if (!optional) {
		printf("Error displaying data directory (optional header)\n");
		return;
	}
	IMAGE_DATA_DIRECTORY currentDirectory;

	for (DWORD i = 0; i < IMAGE_NUMBEROF_DIRECTORY_ENTRIES; i++) {
		const auto& currentDirectory = optional->DataDirectory[i];

		printStruct(12, "IMAGE_DATA_DIRECTORY", directoryNames[i]);
		printField(16, "DWORD", "VirtualAddress", currentDirectory.VirtualAddress);
		printField(16, "DWORD", "Size", currentDirectory.Size);
	}
}

void PEFile::displaySectionHeaders() const {
	const IMAGE_FILE_HEADER* file = fileHeader();
	if (!file) {
		printf("Error displaying section table (file header)\n");
		return;
	}

	const IMAGE_SECTION_HEADER* sectionTable = sectionHeader();

	for (WORD i = 0; i < file->NumberOfSections; i++) {
		const auto& section = sectionTable[i];
		char name[IMAGE_SIZEOF_SHORT_NAME + 1];				// Windows doc states that the name field is not always null terminated
		memcpy(name, section.Name, IMAGE_SIZEOF_SHORT_NAME);
		name[IMAGE_SIZEOF_SHORT_NAME] = '\0';
		printStruct(0, "IMAGE_SECTION_HEADER", name);
		printField(4, "DWORD", "VirtualSize", section.Misc.VirtualSize);
		printField(4, "DWORD", "VirtualAddress", section.VirtualAddress);
		printField(4, "DWORD", "SizeOfRawData", section.SizeOfRawData);
		printField(4, "DWORD", "PointerToRawData", section.PointerToRawData);
		printField(4, "DWORD", "PointerToRelocations", section.PointerToRelocations);
		printField(4, "DWORD", "PointerToLinenumbers", section.PointerToLinenumbers);
		printField(4, "WORD", "NumberOfRelocations", section.NumberOfRelocations);
		printField(4, "WORD", "NumberOfLinenumbers", section.NumberOfLinenumbers);
		printField(4, "DWORD", "Characteristics", section.Characteristics);
	}
}

void PEFile::summary() const {
	displayNtHeaders();
	displaySectionHeaders();
}

template<typename T> void PEFile::printField(int indent, const char* type, const char* name, T value) const {
	printf("%*s%-12s %-30s 0x%0*llX\n",
		indent, "",
		type,
		name,
		static_cast<int>(sizeof(T) * 2),
		static_cast<unsigned long long>(value));
}

void PEFile::printStruct(int indent, const char* type, const char* name) const {
	printf("%*s%-24s %-30s\n", indent, "", type, name);
}