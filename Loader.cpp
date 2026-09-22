// Loader.cpp

#include "Loader.h"
#include "ApiSet.h"

#include <iostream>
#include <filesystem>

LoaderStatus Loader::mapImage(PEFile& pe, ImageContext* ctx) {
	if (ctx == nullptr) {
		std::cout << "InvalidImageContext: loader.mapImage()" << std::endl;

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
	
	if (status == MemoryStatus::AlreadyReserved) {
		status = process_.memory().reserve(
			0,
			pe.ntHeaders()->OptionalHeader.SizeOfImage,
			RegionType::Image,
			&imageBase
		);

		if (status != MemoryStatus::Success) {
			return LoaderStatus::ImageReservationFailed;
		}
	}
	else if (status != MemoryStatus::Success) {
		return LoaderStatus::ImageReservationFailed;
	}

	std::cout << "[+] Image reservation at " << imageBase << std::endl;

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
		//std::cout << "[+] Writing section at " << sectionDst << std::endl;

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
		std::cout << "InvalidImageContext: loader.relocateImage()" << std::endl;
		return LoaderStatus::InvalidImageContext;
	}
	
	if (ctx->baseAddress == ctx->preferredBase) {
		return LoaderStatus::Success;
	}

	std::cout << "Relocating image at " << std::hex << ctx->baseAddress << "(" << std::hex << ctx->preferredBase << ")" << std::endl;

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

	std::cout << " [+] Image at " << std::hex << ctx->baseAddress << " relocated" << std::endl;

	return LoaderStatus::Success;
}

LoaderStatus Loader::resolveImageImports(ImageContext* ctx) {
	
	if (ctx == nullptr) {
		std::cout << "InvalidImageContext: loader.resolveImageImports" << std::endl;
		return LoaderStatus::InvalidImageContext;
	}

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
	
	const IMAGE_DATA_DIRECTORY& importDirectory = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

	if (importDirectory.VirtualAddress == 0) return LoaderStatus::Success;

	uint64_t pImport = ctx->baseAddress + importDirectory.VirtualAddress;


	while (true) {

		IMAGE_IMPORT_DESCRIPTOR import;

		status = process_.memory().read(
			pImport,
			&import,
			sizeof(IMAGE_IMPORT_DESCRIPTOR)
		);

		if (status != MemoryStatus::Success) return LoaderStatus::ImportDescriptorError;

		if (import.Name == 0) {
			break;
		}
		
		uint64_t moduleNameAddr = ctx->baseAddress + import.Name;
		std::string moduleName;

		status = process_.memory().readString(
			moduleNameAddr,
			moduleName,
			260
		);

		if (status != MemoryStatus::Success) {

			std::cout << "ImportDescriptorError in Loader.ResolveImageImports (MemoryManager.ReadString)" << std::endl;

			return LoaderStatus::ImportDescriptorError;
		}

		

		if (ApiSet::isContract(moduleName)) {

			std::wstring wname(moduleName.begin(), moduleName.end());

			auto host = ApiSet::resolve(wname);

			if (!host) {
				std::cout << "Failed to resolve api set: " << moduleName << std::endl;

				return LoaderStatus::ApiSetResolutionFailed;
			}

			moduleName = std::string(host->begin(), host->end());
		}
		

		//std::cout << "[+] Import: " << name << std::endl;

		ImageContext* pModule = process_.findImage(moduleName);

		if (pModule == nullptr) {

			//std::cout << " [+] Loading: "<< name << std::endl;

			LoaderStatus imageStatus = loadModule(moduleName, &pModule);

			if (imageStatus != LoaderStatus::Success) {
				return imageStatus;
			}

			std::cout << " [+] Loaded module: " << moduleName << std::endl;
		}

		if (pModule == nullptr) {
			return LoaderStatus::Other;
		}

		uint64_t hmodule = pModule->baseAddress;

		uint32_t lookupRva = (import.OriginalFirstThunk != 0) ? import.OriginalFirstThunk : import.FirstThunk;
		uint64_t pLookupThunk = ctx->baseAddress + lookupRva;
		uint64_t pIatThunk = ctx->baseAddress + import.FirstThunk;
		
		while (true) {

			IMAGE_THUNK_DATA64 thunk;
			
			status = process_.memory().read(
				pLookupThunk,
				&thunk,
				sizeof(IMAGE_THUNK_DATA64)
			);

			if (status != MemoryStatus::Success) { return LoaderStatus::Other; }

			if (thunk.u1.AddressOfData == 0) {
				break;
			}

			uint64_t resolvedAddress = 0;
			
			if (IMAGE_SNAP_BY_ORDINAL64(thunk.u1.Ordinal)) {
				WORD ordinal = static_cast<WORD>(IMAGE_ORDINAL64(thunk.u1.Ordinal));

				std::cout << "Ordinal import not implemented: " << ordinal << " from " << moduleName << std::endl;
				
				return LoaderStatus::Other;
			}
			else {
				
				uint64_t importByNameAddr = ctx->baseAddress + thunk.u1.AddressOfData;
				std::string functionName;

				status = process_.memory().readString(
					importByNameAddr + sizeof(WORD),
					functionName,
					260
				);

				if (status != MemoryStatus::Success) {
					std::cout << "Error reading import name" << std::endl;
				}

				LoaderStatus ldrStatus = resolveProcAddress(
					hmodule,
					functionName,
					&resolvedAddress
				);

				if (ldrStatus != LoaderStatus::Success) {
					std::cout << "Error resolving " << functionName << " from " << moduleName << std::endl;

					return ldrStatus;
				}
			}

			status = process_.memory().write(
				pIatThunk,
				&resolvedAddress,
				sizeof(resolvedAddress)
			);

			if (status != MemoryStatus::Success) {
				std::cout << "Error writing resolved address to IAT" << std::endl;

				return LoaderStatus::Other;
			}

			pLookupThunk += sizeof(IMAGE_THUNK_DATA64);
			pIatThunk += sizeof(IMAGE_THUNK_DATA64);
		}

		pImport += sizeof(IMAGE_IMPORT_DESCRIPTOR);
	}

	
	return LoaderStatus::Success;
}

LoaderStatus Loader::loadImage(PEFile& pe, ImageContext* ctx) {

	if (ctx == nullptr) {

		std::cout << "InvalidImageContext at loader.loadImage()" << std::endl;
		return LoaderStatus::InvalidImageContext;
	}

	ctx->state = ImageState::Loading;

	LoaderStatus status = mapImage(pe, ctx);
	if (status != LoaderStatus::Success) {
		return status;
	}

	status = relocateImage(ctx);
	if (status != LoaderStatus::Success) {
		return status;
	}

	status = resolveImageImports(ctx);
	if (status != LoaderStatus::Success) {
		return status;
	}

	ctx->state = ImageState::Loaded;

	return LoaderStatus::Success;
}

LoaderStatus Loader::loadExecutable(std::filesystem::path path) {

	PEFile image(path);

	ImageContext* ctx = process_.addImage(path.filename().string());
	if (ctx == nullptr) {

		std::cout << "InvalidImageContext at loader.loadExecutable" << std::endl;
		return LoaderStatus::InvalidImageContext;
	}

	ctx->type = ImageType::Executable;
	ctx->path = path;

	process_.setMainImage(ctx);


	std::cout << "Added: " << ctx << '\n';
	std::cout << "Found: "
		<< process_.findImage(path.filename().string())
		<< '\n';

	return loadImage(image, ctx);
}

LoaderStatus Loader::loadModule(const std::string& name, ImageContext** ctxOut) {

	if (ctxOut == nullptr) {

		std::cout << "InvalidImageContext: loader.loadModule()" << std::endl;
		return LoaderStatus::InvalidImageContext;
	}
	
	ImageContext* ctx = process_.findImage(name);

	if (ctx != nullptr) {
		*ctxOut = ctx;
		return LoaderStatus::Success;
	}

	auto pathOpt = findImagePath(name);
	if (!pathOpt) {
		return LoaderStatus::FailedToLocateImage;
	}
	else {
		std::cout << *pathOpt << std::endl;
	}

	PEFile image(*pathOpt);

	ctx = process_.addImage(name);
	ctx->type = ImageType::DLL;
	ctx->path = pathOpt.value();

	LoaderStatus status = loadImage(image, ctx);
	if (status != LoaderStatus::Success) {
		return status;
	}

	*ctxOut = ctx;

	return LoaderStatus::Success;
}

std::optional<std::filesystem::path> Loader::findImagePath(const std::string& name) {

	std::vector<std::filesystem::path> searchPaths;

	ImageContext* mainImage = process_.mainImage();

	if (mainImage != nullptr && !mainImage->path.empty()) {
		searchPaths.push_back(mainImage->path.parent_path());
	}

	wchar_t systemBuffer[MAX_PATH];
	if (GetSystemDirectoryW(systemBuffer, MAX_PATH) != 0) {
		searchPaths.emplace_back(systemBuffer);
	}

	wchar_t windowsBuffer[MAX_PATH];
	if (GetWindowsDirectoryW(windowsBuffer, MAX_PATH) != 0) {
		searchPaths.emplace_back(windowsBuffer);
	}

	searchPaths.push_back(std::filesystem::current_path());

	for (const auto& directory : searchPaths) {

		std::filesystem::path candidate = directory / name;

		if (std::filesystem::is_regular_file(candidate)) {
			return candidate;
		}
	}

	std::cout << "Image " << name << " not found" << std::endl;

	return std::nullopt;


}

LoaderStatus Loader::resolveProcAddress(uint64_t hmodule, std::string name, uint64_t* addrOut) {

	if (hmodule == 0 || name.empty() || addrOut == nullptr) {
		return LoaderStatus::Other;
	}

	IMAGE_DOS_HEADER dos;

	MemoryStatus status = process_.memory().read(
		hmodule,
		&dos,
		sizeof(IMAGE_DOS_HEADER)
	);

	if (status != MemoryStatus::Success) {
		return LoaderStatus::Other;
	}

	if (dos.e_magic != IMAGE_DOS_SIGNATURE) {
		return LoaderStatus::InvalidImage;
	}

	IMAGE_NT_HEADERS64 nt;

	status = process_.memory().read(
		hmodule + dos.e_lfanew,
		&nt,
		sizeof(IMAGE_NT_HEADERS64)
	);

	if (status != MemoryStatus::Success) {
		return LoaderStatus::Other;
	}

	if (nt.Signature != IMAGE_NT_SIGNATURE) {
		return LoaderStatus::InvalidImage;
	}

	const IMAGE_DATA_DIRECTORY& exportData = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];

	DWORD exportDirRva = exportData.VirtualAddress;
	DWORD exportDirSize = exportData.Size;

	if (exportDirRva == 0 || exportDirSize == 0) {
		return LoaderStatus::Other;
	}

	IMAGE_EXPORT_DIRECTORY exportDir{};

	status = process_.memory().read(
		hmodule + exportDirRva,
		&exportDir,
		sizeof(IMAGE_EXPORT_DIRECTORY)
	);

	if (status != MemoryStatus::Success) {
		return LoaderStatus::Other;
	}

	uint64_t names = hmodule + exportDir.AddressOfNames;
	uint64_t functions = hmodule + exportDir.AddressOfFunctions;
	uint64_t ordinals = hmodule + exportDir.AddressOfNameOrdinals;

	for (DWORD i = 0; i < exportDir.NumberOfNames; ++i) {

		DWORD nameRva;

		status = process_.memory().read(
			names + sizeof(DWORD) * i,
			&nameRva,
			sizeof(nameRva)
		);

		if (status != MemoryStatus::Success) {
			return LoaderStatus::Other;
		}

		std::string functionName;

		status = process_.memory().readString(
			hmodule + nameRva,
			functionName,
			260
		);

		if (status != MemoryStatus::Success) {
			return LoaderStatus::Other;
		}

		if (functionName != name) {
			continue;
		}

		std::cout << "Found function: " << name << std::endl;

		WORD functionIndex;

		status = process_.memory().read(
			ordinals + sizeof(WORD) * i,
			&functionIndex,
			sizeof(functionIndex)
		);

		if (status != MemoryStatus::Success) {
			return LoaderStatus::Other;
		}

		if (functionIndex >= exportDir.NumberOfFunctions) {
			return LoaderStatus::Other;
		}

		DWORD functionRva;

		status = process_.memory().read(
			functions + sizeof(DWORD) * functionIndex,
			&functionRva,
			sizeof(functionRva)
		);

		if (status != MemoryStatus::Success) {
			return LoaderStatus::Other;
		}

		uint64_t exportEnd = static_cast<uint64_t>(exportDirRva) + exportDirSize;

		if (functionRva >= exportDirRva && functionRva < exportEnd) {
			return LoaderStatus::Other;
		}

		*addrOut = hmodule + functionRva;

		return LoaderStatus::Success;
	}

	std::cout << "Failed to resolve address for " << name << std::endl; 

	return LoaderStatus::ExportNotFound;
}