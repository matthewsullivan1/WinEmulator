// Loader.cpp

#include "Loader.h"

LoaderStatus Loader::mapImage(PEFile& pe, ImageContext* ctx) {
	if (ctx == nullptr) {
		return LoaderStatus::InvalidImageContext;
	}

	// Reserve region for entire image
	uint64_t imageBase = 0;

	MemoryStatus status = process_.memory().reserve(
		pe.ntHeaders()->OptionalHeader.ImageBase, 
		pe.ntHeaders()->OptionalHeader.SizeOfImage, 
		RegionType::Image,
		&imageBase
	);
	
	if (status != MemoryStatus::Success) {
		return LoaderStatus::ImageReservationFailed;
	}

	// Commit header subregion
	status = process_.memory().commit(
		imageBase,
		pe.ntHeaders()->OptionalHeader.SizeOfHeaders,
		Protection{
			.r = true,
			.w = true,
			.x = false
		}
	);
	
	if (status != MemoryStatus::Success) {

		process_.memory().release(imageBase);

		return LoaderStatus::ImageHeaderCommitFailed;
	}

	// Write headers
	status = process_.memory().write(
		imageBase,
		pe.data().data(),
		pe.ntHeaders()->OptionalHeader.SizeOfHeaders
	);

	if (status != MemoryStatus::Success) {

		process_.memory().release(imageBase);

		return LoaderStatus::ImageHeaderMapFailed;
	}

	auto fileData = pe.data();

	for (const auto& section : pe.sections()) {
		uint64_t sectionDst = imageBase + section.VirtualAddress;

		auto sectionData = fileData.subspan(
			section.PointerToRawData,
			section.SizeOfRawData
		);

		size_t mappedSize = std::max(
			static_cast<size_t>(section.Misc.VirtualSize),
			static_cast<size_t>(section.SizeOfRawData)
		);

		status = process_.memory().commit(
			sectionDst,
			mappedSize,
			Protection{
				.r = true,
				.w = true,
				.x = false
			}
		);

		if (status != MemoryStatus::Success) {

			process_.memory().release(imageBase);

			return LoaderStatus::ImageSectionCommitFailed;
		}

		status = process_.memory().write(
			sectionDst,
			sectionData.data(),
			sectionData.size()
		);

		if (status != MemoryStatus::Success) {
			
			process_.memory().release(imageBase);

			return LoaderStatus::ImageSectionMapFailed;
		}
	}

	ctx->baseAddress = imageBase;
	ctx->preferredBase = pe.ntHeaders()->OptionalHeader.ImageBase;
	ctx->entryPoint = imageBase + pe.entryPointRva();
	ctx->size = pe.ntHeaders()->OptionalHeader.SizeOfImage;

	return LoaderStatus::Success;
}

LoaderStatus Loader::relocateImage(ImageContext* ctx) {
	if (ctx == nullptr) {
		return LoaderStatus::InvalidImageContext;
	}
	
	if (ctx->baseAddress == ctx->preferredBase) {
		return LoaderStatus::Success;
	}

	IMAGE_DOS_HEADER dos;
	MemoryStatus status = process_.memory().read(
		ctx->baseAddress,
		&dos,
		sizeof(IMAGE_DOS_HEADER)
	);

	if (status != MemoryStatus::Success) {
		return LoaderStatus::ImageRelocationFailed;
	}

	IMAGE_NT_HEADERS64 nt;
	status = process_.memory().read(
		ctx->baseAddress + dos.e_lfanew,
		&nt,
		sizeof(IMAGE_NT_HEADERS64)
	);
	
	if (status != MemoryStatus::Success) {
		return LoaderStatus::ImageRelocationFailed;
	}

	if (nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size == 0) {
		return LoaderStatus::Success;
	}

	IMAGE_DATA_DIRECTORY relocationBase = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
	
	if (relocationBase.VirtualAddress == 0 || relocationBase.Size == 0) {
		return LoaderStatus::Success;
	}

	uint64_t delta = ctx->baseAddress - ctx->preferredBase;
	uint64_t blockAddress = ctx->baseAddress + relocationBase.VirtualAddress;
	uint64_t relocationEnd = blockAddress + relocationBase.Size;


	while (blockAddress < relocationEnd) {
		
		IMAGE_BASE_RELOCATION block;

		status = process_.memory().read(
			blockAddress,
			&block,
			sizeof(IMAGE_BASE_RELOCATION)
		);

		if (status != MemoryStatus::Success) return LoaderStatus::ImageRelocationFailed;
		if (block.SizeOfBlock < sizeof(IMAGE_BASE_RELOCATION)) return LoaderStatus::ImageRelocationFailed;

		size_t count = (block.SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(uint16_t);

		for (size_t i = 0; i < count; i++) {
			
			uint16_t entry;

			status = process_.memory().read(
				blockAddress + sizeof(IMAGE_BASE_RELOCATION) + (i * sizeof(entry)),
				&entry,
				sizeof(entry)
			);

			if (status != MemoryStatus::Success) return LoaderStatus::ImageRelocationFailed;

			uint16_t type = entry >> 12;
			uint16_t offset = entry & 0x0FFF;

			if (type == IMAGE_REL_BASED_ABSOLUTE) {
				continue;
			}

			if (type != IMAGE_REL_BASED_DIR64) { // only relocation that 64 bit PEs use (aside from padding) 
				return LoaderStatus::UnsupportedRelocationType;
			}

			uint64_t patchAddr = ctx->baseAddress + block.VirtualAddress + offset;
			uint64_t fix;

			status = process_.memory().read(
				patchAddr,
				&fix,
				sizeof(uint64_t)
			);

			if (status != MemoryStatus::Success) return LoaderStatus::ImageRelocationFailed;

			fix += delta;

			status = process_.memory().write(
				patchAddr,
				&fix,
				sizeof(uint64_t)
			);

			if (status != MemoryStatus::Success) return LoaderStatus::ImageRelocationFailed;
		}

		blockAddress += block.SizeOfBlock;

	}

	return LoaderStatus::Success;
}



LoaderStatus Loader::resolveImageImports(ImageContext* ctx) {
	
	if (ctx == nullptr) return LoaderStatus::InvalidImageContext;

	IMAGE_DOS_HEADER dos;
	IMAGE_NT_HEADERS64 nt;

	MemoryStatus status = process_.memory().read(
		ctx->baseAddress,
		&dos,
		sizeof(IMAGE_DOS_HEADER)
	);

	if (status != MemoryStatus::Success) return LoaderStatus::InvalidImage;

	status = process_.memory().read(
		ctx->baseAddress + dos.e_lfanew,
		&nt,
		sizeof(IMAGE_NT_HEADERS64)
	);

	if (status != MemoryStatus::Success) return LoaderStatus::InvalidImage;
	
	IMAGE_DATA_DIRECTORY importDirectoryTable = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

	if (importDirectoryTable.Size == 0 || importDirectoryTable.VirtualAddress == 0) return LoaderStatus::Success;

	uint64_t pImport = ctx->baseAddress + importDirectoryTable.VirtualAddress;
	IMAGE_IMPORT_DESCRIPTOR import;
	status = process_.memory().read(
		pImport,
		&import,
		sizeof(IMAGE_IMPORT_DESCRIPTOR)
	);

	if (status != MemoryStatus::Success) return LoaderStatus::ImportDescriptorError;


	// We can find a module by
	// 1. is already loaded, handle is in process_.modules_
	// 2. is being loaded, denoted by existence in process_.modules_, but status is loading
	// 3. found on host disk via standard windows search paths
	//		3.a and then is mapped
	// 4. found in modules list, but has failed status -- prog should exit before a module is 'found' this way
	// 

	while (import.Name != 0) {
		
		uint64_t nameAddr = ctx->baseAddress + import.Name;
		std::string name;

		status = process_.memory().readString(
			nameAddr,
			name,
			260
		);
		
		if (status != MemoryStatus::Success) return LoaderStatus::ImportDescriptorError;

		Module* pModule = process_.findModule(name);

		if (pModule != nullptr && pModule->state == ModuleState::Loaded) {
			
		}

		Module& m = process_.addModule({ .name = name });

	}

	

}
