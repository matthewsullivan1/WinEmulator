// Loader.cpp

#include "Loader.h"
#include "ApiSet.h"

#include <iostream>
#include <filesystem>

LoaderStatus Loader::mapImage(PEFile& pe, ImageContext* ctx) {
	if (ctx == nullptr) {
		return LoaderStatus::InvalidImageContext;
	}

	uint32_t alignment = pe.optionalHeader()->SectionAlignment;

	if (alignment != PAGE_SIZE || alignment % PAGE_SIZE != 0) {
		return LoaderStatus::UnsupportedSectionAlignment;
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

	// Commit header subregion
	status = process_.memory().commit(
		imageBase,
		pe.ntHeaders()->OptionalHeader.SizeOfHeaders,
		PageProtection::ReadWrite
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

		if (section.VirtualAddress % pe.optionalHeader()->SectionAlignment != 0) {
			return LoaderStatus::InvalidSectionAlignment;
		}
		
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
			PageProtection::ReadWrite
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

	std::cout
		<< "[RELOC] " << ctx->name
		<< " 0x" << std::hex << ctx->preferredBase
		<< " -> 0x" << ctx->baseAddress
		<< std::dec << '\n';

	return LoaderStatus::Success;
}

LoaderStatus Loader::resolveImageImports(ImageContext* ctx) {
	
	if (ctx == nullptr) {
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

			std::cout
				<< "[ERROR] Failed to read import module name\n"
				<< "        image:  " << ctx->name << '\n'
				<< "        status: " << static_cast<int>(status) << '\n';

			return LoaderStatus::ImportDescriptorError;
		}

		if (ApiSet::isContract(moduleName)) {

			std::string contractName = moduleName;

			std::wstring wname(
				moduleName.begin(),
				moduleName.end()
			);

			std::wstring parentName(
				ctx->name.begin(),
				ctx->name.end()
			);

			auto host = ApiSet::resolve(
				wname,
				parentName
			);

			if (!host) {
				std::cout
					<< "[ERROR] API-set resolution failed\n"
					<< "        image:    " << ctx->name << '\n'
					<< "        contract: " << contractName << '\n';

				return LoaderStatus::ApiSetResolutionFailed;
			}

			moduleName = std::string(host->begin(), host->end());

			std::cout
				<< "[APISET] " << contractName
				<< " -> " << moduleName << '\n';
		}

		ImageContext* pModule = process_.findImage(moduleName);

		if (pModule == nullptr) {

			LoaderStatus imageStatus = loadModule(moduleName, &pModule);

			if (imageStatus != LoaderStatus::Success) {
				std::cout
					<< "[ERROR] Failed to load dependency\n"
					<< "        image:  " << ctx->name << '\n'
					<< "        module: " << moduleName << '\n'
					<< "        status: "
					<< loaderStatusString(imageStatus) << '\n';

				return imageStatus;
			}
		}

		if (pModule == nullptr) {
			std::cout
				<< "[ERROR] Module loaded without valid image context\n"
				<< "        image:  " << ctx->name << '\n'
				<< "        module: " << moduleName << '\n';

			return LoaderStatus::InvalidModuleContext;
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

			if (status != MemoryStatus::Success) { 
				std::cout
					<< "[ERROR] Failed to read import thunk\n"
					<< "        image:  " << ctx->name << '\n'
					<< "        module: " << moduleName << '\n'
					<< "        thunk:  0x"
					<< std::hex << pLookupThunk
					<< std::dec << '\n';

				return LoaderStatus::ImportThunkReadFailed;
			}

			if (thunk.u1.AddressOfData == 0) {
				break;
			}

			uint64_t resolvedAddress = 0;
			
			if (IMAGE_SNAP_BY_ORDINAL64(thunk.u1.Ordinal)) {
				WORD ordinal = static_cast<WORD>(IMAGE_ORDINAL64(thunk.u1.Ordinal));

				LoaderStatus ldrStatus = resolveProcAddress(
					hmodule,
					ordinal,
					&resolvedAddress
				);

				if (ldrStatus != LoaderStatus::Success) {
					std::cout
						<< "[ERROR] Ordinal import resolution failed\n"
						<< "        image:   " << ctx->name << '\n'
						<< "        module:  " << moduleName << '\n'
						<< "        ordinal: " << ordinal << '\n'
						<< "        status:  "
						<< loaderStatusString(ldrStatus) << '\n';
					
					
					return ldrStatus;
				
				}
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
					std::cout
						<< "[ERROR] Failed to read import name\n"
						<< "        image:   " << ctx->name << '\n'
						<< "        module:  " << moduleName << '\n'
						<< "        address: 0x"
						<< std::hex << importByNameAddr
						<< std::dec << '\n';

					return LoaderStatus::ImportNameReadFailed;
				}

				LoaderStatus ldrStatus = resolveProcAddress(
					hmodule,
					functionName,
					&resolvedAddress
				);

				if (ldrStatus != LoaderStatus::Success) {
					std::cout
						<< "[ERROR] Import resolution failed\n"
						<< "        image:    " << ctx->name << '\n'
						<< "        module:   " << moduleName << '\n'
						<< "        function: " << functionName << '\n'
						<< "        status:   "
						<< loaderStatusString(ldrStatus) << '\n';

					return ldrStatus;
				}
			}

			status = process_.memory().write(
				pIatThunk,
				&resolvedAddress,
				sizeof(resolvedAddress)
			);

			if (status != MemoryStatus::Success) {
				std::cout
					<< "[ERROR] Failed to write IAT entry\n"
					<< "        image:   " << ctx->name << '\n'
					<< "        module:  " << moduleName << '\n'
					<< "        address: 0x"
					<< std::hex << pIatThunk
					<< std::dec << '\n';

				return LoaderStatus::IatWriteFailed;
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
		std::cout << "[ERROR] Invalid image context in loadImage()\n";
		return LoaderStatus::InvalidImageContext;
	}

	std::cout
		<< "[LOAD] " << ctx->name
		<< '\n';

	ctx->state = ImageState::Loading;

	LoaderStatus status = mapImage(pe, ctx);
	if (status != LoaderStatus::Success) {
		std::cout
			<< "[ERROR] Failed to map " << ctx->name
			<< " (status " << loaderStatusString(status) << ")\n";
		return status;
	}

	std::cout
		<< "       base: 0x" << std::hex << ctx->baseAddress
		<< std::dec << '\n';

	status = relocateImage(ctx);
	if (status != LoaderStatus::Success) {
		std::cout
			<< "[ERROR] Failed to relocate " << ctx->name
			<< " (status " << loaderStatusString(status) << ")\n";
		return status;
	}

	status = resolveImageImports(ctx);
	if (status != LoaderStatus::Success) {
		std::cout
			<< "[ERROR] Failed imports for " << ctx->name
			<< " (status " << loaderStatusString(status) << ")\n";
		return status;
	}

	ctx->state = ImageState::Loaded;

	std::cout
		<< "[DONE] " << ctx->name
		<< '\n';

	return LoaderStatus::Success;
}

LoaderStatus Loader::loadExecutable(std::filesystem::path path) {

	PEFile image(path);

	ImageContext* ctx = process_.addImage(path.filename().string());
	if (ctx == nullptr) {
		return LoaderStatus::InvalidImageContext;
	}

	ctx->type = ImageType::Executable;
	ctx->path = path;

	process_.setMainImage(ctx);

	return loadImage(image, ctx);
}

LoaderStatus Loader::loadModule(const std::string& name, ImageContext** ctxOut) {

	if (ctxOut == nullptr) {
		return LoaderStatus::InvalidImageContext;
	}
	
	ImageContext* ctx = process_.findImage(name);

	if (ctx != nullptr) {
		*ctxOut = ctx;
		return LoaderStatus::Success;
	}

	auto pathOpt = findImagePath(name);
	if (!pathOpt) {
		std::cout
			<< "[ERROR] Failed to locate module\n"
			<< "        module: " << name << '\n';

		return LoaderStatus::FailedToLocateImage;
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

	return std::nullopt;

}

LoaderStatus Loader::getExportContext(uint64_t hmodule, ExportContext& out) {
	if (hmodule == 0) {
		return LoaderStatus::InvalidImage;
	}

	IMAGE_DOS_HEADER dos;

	MemoryStatus status = process_.memory().read(
		hmodule,
		&dos,
		sizeof(dos)
	);

	if (status != MemoryStatus::Success) {
		return LoaderStatus::ExportDirectoryReadFailed;
	}

	if (dos.e_magic != IMAGE_DOS_SIGNATURE) {
		return LoaderStatus::InvalidImage;
	}

	IMAGE_NT_HEADERS64 nt;

	status = process_.memory().read(
		hmodule + dos.e_lfanew,
		&nt,
		sizeof(nt)
	);

	if (status != MemoryStatus::Success) {
		return LoaderStatus::ExportDirectoryReadFailed;
	}

	if (nt.Signature != IMAGE_NT_SIGNATURE) {
		return LoaderStatus::InvalidImage;
	}

	const IMAGE_DATA_DIRECTORY& exportData = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];


	if (exportData.VirtualAddress == 0 || exportData.Size == 0) {
		return LoaderStatus::ExportNotFound;
	}

	out.directoryRva = exportData.VirtualAddress;
	out.directorySize = exportData.Size;

	status = process_.memory().read(
		hmodule + out.directoryRva,
		&out.directory,
		sizeof(out.directory)
	);

	if (status != MemoryStatus::Success) {
		return LoaderStatus::ExportDirectoryReadFailed;
	}

	return LoaderStatus::Success;
}

LoaderStatus Loader::resolveProcAddress(uint64_t hmodule, const std::string& name, uint64_t* addrOut, size_t depth) {
	
	if (hmodule == 0 || name.empty() || addrOut == nullptr) {
		return LoaderStatus::ExportTableReadFailed;
	}

	ExportContext exports;

	LoaderStatus status = getExportContext(hmodule, exports);

	if (status != LoaderStatus::Success) {
		return status;
	}

	const IMAGE_EXPORT_DIRECTORY& dir = exports.directory;

	uint64_t names = hmodule + dir.AddressOfNames;
	uint64_t functions = hmodule + dir.AddressOfFunctions;
	uint64_t ordinals = hmodule + dir.AddressOfNameOrdinals;

	for (DWORD i = 0; i < dir.NumberOfNames; ++i) {

		DWORD nameRva;

		MemoryStatus memStatus = process_.memory().read(
			names + sizeof(DWORD) * i,
			&nameRva,
			sizeof(nameRva)
		);

		if (memStatus != MemoryStatus::Success) {
			return LoaderStatus::ExportTableReadFailed;
		}

		std::string functionName;

		memStatus = process_.memory().readString(
			hmodule + nameRva,
			functionName,
			260
		);

		if (memStatus != MemoryStatus::Success) {
			return LoaderStatus::ExportTableReadFailed;
		}

		if (functionName != name) {
			continue;
		}

		WORD functionIndex;

		memStatus = process_.memory().read(
			ordinals + sizeof(WORD) * i,
			&functionIndex,
			sizeof(functionIndex)
		);

		if (memStatus != MemoryStatus::Success) {
			return LoaderStatus::ExportTableReadFailed;
		}

		if (functionIndex >= dir.NumberOfFunctions) {
			return LoaderStatus::ExportNotFound;
		}

		DWORD functionRva;

		memStatus = process_.memory().read(
			functions + sizeof(DWORD) * functionIndex,
			&functionRva,
			sizeof(functionRva)
		);

		if (memStatus != MemoryStatus::Success) {
			return LoaderStatus::ExportTableReadFailed;
		}

		return resolveExportRva(
			hmodule,
			functionRva,
			exports.directoryRva,
			exports.directorySize,
			addrOut,
			depth
		);
	}

	return LoaderStatus::ExportNotFound;
}

LoaderStatus Loader::resolveProcAddress(uint64_t hmodule, WORD ordinal, uint64_t* addrOut, size_t depth) {
	
	if (hmodule == 0 || addrOut == nullptr) {
		return LoaderStatus::ExportTableReadFailed;
	}

	ExportContext exports;

	LoaderStatus status = getExportContext(hmodule, exports);

	if (status != LoaderStatus::Success) {
		return status;
	}

	const IMAGE_EXPORT_DIRECTORY& dir = exports.directory;

	if (ordinal < dir.Base) {
		return LoaderStatus::ExportNotFound;
	}

	DWORD functionIndex = static_cast<DWORD>(ordinal - dir.Base);

	if (functionIndex >= dir.NumberOfFunctions) {
		return LoaderStatus::ExportNotFound;
	}

	DWORD functionRva;

	MemoryStatus memStatus = process_.memory().read(
		hmodule +
		dir.AddressOfFunctions +
		sizeof(DWORD) * functionIndex,
		&functionRva,
		sizeof(functionRva)
	);

	if (memStatus != MemoryStatus::Success) {
		return LoaderStatus::ExportTableReadFailed;
	}

	return resolveExportRva(
		hmodule,
		functionRva,
		exports.directoryRva,
		exports.directorySize,
		addrOut,
		depth
	);
}

LoaderStatus Loader::resolveExportRva(
	uint64_t hmodule,
	DWORD functionRva,
	DWORD exportDirRva,
	DWORD exportDirSize,
	uint64_t* addrOut,
	size_t depth)
{
	if (addrOut == nullptr) {
		return LoaderStatus::InvalidArgument;
	}

	uint64_t exportEnd = static_cast<uint64_t>(exportDirRva) + exportDirSize;

	// RVA inside export directory = forwarded export
	if (functionRva >= exportDirRva && functionRva < exportEnd) {
		
		std::string forwarder;

		MemoryStatus status = process_.memory().readString(
			hmodule + functionRva,
			forwarder,
			260
		);

		if (status != MemoryStatus::Success) {
			return LoaderStatus::ForwarderReadFailed;
		}

		return resolveForwarder(
			forwarder,
			hmodule,
			addrOut,
			depth + 1
		);
	}

	*addrOut = hmodule + functionRva;

	return LoaderStatus::Success;
}

LoaderStatus Loader::resolveForwarder(const std::string& forwarder, uint64_t parentModuleBase, uint64_t* addrOut, size_t depth) {
	constexpr size_t MAX_FORWARD_DEPTH = 32;

	if (addrOut == nullptr || forwarder.empty()) {
		return LoaderStatus::InvalidArgument;
	}

	if (depth > MAX_FORWARD_DEPTH) {
		std::cout
			<< "[ERROR] Maximum forwarder depth exceeded\n"
			<< "        forwarder: " << forwarder << '\n'
			<< "        depth:     " << depth << '\n';

		return LoaderStatus::ForwarderDepthExceeded;
	}

	size_t dot = forwarder.find('.');

	if (dot == std::string::npos ||
		dot == 0 ||
		dot == forwarder.size() - 1)
	{
		std::cout
			<< "[ERROR] Invalid forwarder string\n"
			<< "        forwarder: " << forwarder << '\n';

		return LoaderStatus::InvalidForwarder;
	}

	std::string moduleName = forwarder.substr(0, dot);
	std::string exportName = forwarder.substr(dot + 1);

	std::cout
		<< "[FWD] " << forwarder
		<< " (depth " << depth << ")\n";

	// Forwarder strings generally omit the extension
	if (!moduleName.ends_with(".dll")) {
		moduleName += ".dll";
	}

	ImageContext* parentModule = process_.findImageByBase(parentModuleBase);

	std::wstring parentName;

	if (parentModule != nullptr) {
		parentName.assign(
			parentModule->name.begin(),
			parentModule->name.end()
		);
	}

	// Handle API-set contracts too
	if (ApiSet::isContract(moduleName)) {

		std::wstring wname(
			moduleName.begin(),
			moduleName.end()
		);

		auto host = ApiSet::resolve(
			wname,
			parentName
		);

		if (!host) {
			return LoaderStatus::ApiSetResolutionFailed;
		}

		moduleName = std::string(
			host->begin(),
			host->end()
		);
	}

	ImageContext* module = process_.findImage(moduleName);

	if (module == nullptr) {

		LoaderStatus status = loadModule(moduleName, &module);

		if (status != LoaderStatus::Success) {
			return status;
		}
	}

	if (module == nullptr) {
		return LoaderStatus::ForwarderError;
	}

	// Ordinal forwarder
	if (exportName.starts_with("#")) {

		if (exportName.size() == 1) {
			return LoaderStatus::ForwarderError;
		}

		try {
			unsigned long value = std::stoul(exportName.substr(1));

			if (value > UINT16_MAX) {
				return LoaderStatus::ForwarderError;
			}

			WORD ordinal = static_cast<WORD>(value);

			return resolveProcAddress(
				module->baseAddress,
				ordinal,
				addrOut,
				depth
			);
		}
		catch (...) {
			return LoaderStatus::ForwarderError;
		}
	}

	return resolveProcAddress(
		module->baseAddress,
		exportName,
		addrOut,
		depth
	);
}

LoaderStatus Loader::setSectionProtections() {

	uint64_t pebAddress = process_.getPeb();
	
	ImageContext* ctx = process_.mainImage();
	uint64_t moduleBase = ctx->baseAddress;

	printf("%llX\n", moduleBase);

	//for (auto& it : process_.images_) {

	//}

	return LoaderStatus::Success;
}

