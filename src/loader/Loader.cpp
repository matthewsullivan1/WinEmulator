// Loader.cpp

#include "Loader.h"
#include "ApiSet.h"

#include <iostream>
#include <filesystem>

using win::IMAGE_DOS_HEADER;
using win::IMAGE_NT_HEADERS;


using win::IMAGE_SECTION_HEADER;
using win::IMAGE_DATA_DIRECTORY;
using win::IMAGE_BASE_RELOCATION;

using win::IMAGE_IMPORT_DESCRIPTOR;
using win::IMAGE_THUNK_DATA64;
using win::IMAGE_SNAP_BY_ORDINAL64;
using win::IMAGE_ORDINAL64;

using win::IMAGE_DATA_DIRECTORY;
using win::IMAGE_EXPORT_DIRECTORY;
using win::IMAGE_DIRECTORY_ENTRY_EXPORT;


using win::IMAGE_DIRECTORY_ENTRY_BASERELOC;

using win::WORD;
using win::DWORD;

namespace emu {


LoaderStatus Loader::loadImage(const PEFile& pe, Module& module) {


	std::cout
		<< "[LOAD] " << module.name
		<< '\n';

	module.state = ModuleState::Loading;

	LoaderStatus status = mapImage(pe, module);
	if (status != LoaderStatus::Success) {
		std::cout
			<< "[ERROR] Failed to map " << module.name
			<< " (status " << loaderStatusString(status) << ")\n";
		return status;
	}

	status = relocateImage(pe, module);
	if (status != LoaderStatus::Success) {
		std::cout
			<< "[ERROR] Failed to relocate " << module.name
			<< " (status " << loaderStatusString(status) << ")\n";
		return status;
	}

	//status = resolveImageImports(pe, module);
	//if (status != LoaderStatus::Success) {
	//	std::cout
	//		<< "[ERROR] Failed imports for " << module.name
	//		<< " (status " << loaderStatusString(status) << ")\n";
	//	return status;
	//}


	// Move to after protections are set
	module.state = ModuleState::Loaded;

	std::cout
		<< "[DONE] " << module.name
		<< '\n';

	return LoaderStatus::Success;
}


LoaderStatus Loader::mapImage(const PEFile& pe, Module& module) {

	if (pe.ntHeaders().OptionalHeader.SectionAlignment != PAGE_SIZE) {
		return LoaderStatus::UnsupportedSectionAlignment;
	}

	auto nt = pe.ntHeaders();

	// Reserve region for entire image
	ptr_t imageBase = 0;

	// Attempt to reserve region @ preferred base address
	MemoryStatus status = process_.memory().reserve(
		nt.OptionalHeader.ImageBase,
		nt.OptionalHeader.SizeOfImage,
		RegionType::Image,
		PageProtection::ReadWrite,
		&imageBase
	);
	
	// Let memory manager pick an address (preferred base isnt available)
	if (status == MemoryStatus::AlreadyReserved) {
		status = process_.memory().reserve(
			0,
			nt.OptionalHeader.SizeOfImage,
			RegionType::Image,
			PageProtection::ReadWrite,
			&imageBase
		);
	}
	
	if (status != MemoryStatus::Success) {
		return LoaderStatus::ImageReservationFailed;
	}

	// Commit header subregion
	status = process_.memory().commit(
		imageBase,
		nt.OptionalHeader.SizeOfHeaders,
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
		nt.OptionalHeader.SizeOfHeaders
	);

	if (status != MemoryStatus::Success) {

		process_.memory().release(imageBase);

		return LoaderStatus::ImageHeaderMapFailed;
	}

	auto fileData = pe.data();

	for (size_t i = 0; i < nt.FileHeader.NumberOfSections; i++) {

		auto sectionOpt = pe.sectionHeader(i);
		
		if (!sectionOpt) {

			process_.memory().release(module.baseAddress);
			return LoaderStatus::SectionReadFailed;
		}

		IMAGE_SECTION_HEADER section = *sectionOpt;
		
		uint64_t sectionDst = imageBase + section.VirtualAddress;

		// Check for zero size? 
		auto sectionData = fileData.subspan(
			section.PointerToRawData,
			section.SizeOfRawData
		);

		size_t mappedSize = std::max(
			static_cast<size_t>(section.Misc.VirtualSize),
			static_cast<size_t>(section.SizeOfRawData)
		);

		// Commit regardless of size of raw data
		status = process_.memory().commit(
			sectionDst,
			mappedSize,
			PageProtection::ReadWrite
		);

		if (status != MemoryStatus::Success) {

			process_.memory().release(imageBase);

			return LoaderStatus::ImageSectionCommitFailed;
		}

		// Only write when data exists
		if (section.SizeOfRawData != 0) {
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

	}

	module.baseAddress = imageBase;

	// ptr_t(0) reserved for modules with no entry point
	module.entryPoint = nt.OptionalHeader.AddressOfEntryPoint == 0
		? 0
		: imageBase + nt.OptionalHeader.AddressOfEntryPoint;

	module.sizeOfImage = pe.ntHeaders().OptionalHeader.SizeOfImage;

	return LoaderStatus::Success;
}

LoaderStatus Loader::relocateImage(const PEFile& pe, Module& module) {

	auto nt = pe.ntHeaders();
	ptr_t preferredBase = nt.OptionalHeader.ImageBase;
	
	if (module.baseAddress == preferredBase) {
		return LoaderStatus::Success;
	}

	// Relocs stripped & image isnt loaded at preferred base
	if (nt.FileHeader.Characteristics & win::IMAGE_FILE_RELOCS_STRIPPED) {
		return LoaderStatus::ImageRelocationFailed;
	}

	if (nt.OptionalHeader.NumberOfRvaAndSizes <= win::IMAGE_DIRECTORY_ENTRY_BASERELOC) {
		return LoaderStatus::ImageRelocationFailed;
	}

	auto relocationBase = nt.OptionalHeader.DataDirectory[win::IMAGE_DIRECTORY_ENTRY_BASERELOC];

	if (relocationBase.VirtualAddress == 0 || relocationBase.Size == 0) {
		return LoaderStatus::ImageRelocationFailed;
	}

	if (!validImageRange(
		relocationBase.VirtualAddress,
		relocationBase.Size,
		module.sizeOfImage
	)) {
		return LoaderStatus::ImageRelocationFailed;
	}


	uint64_t delta = module.baseAddress - preferredBase;
	ptr_t blockAddress = module.baseAddress + relocationBase.VirtualAddress;
	ptr_t relocationEnd = blockAddress + relocationBase.Size;

	ptr_t directoryAddress = module.baseAddress + relocationBase.VirtualAddress;
	uint64_t offset = 0;


	while (offset < relocationBase.Size) {

		uint64_t remaining = relocationBase.Size - offset;

		if (remaining < sizeof(IMAGE_BASE_RELOCATION)) {
			return LoaderStatus::ImageRelocationFailed;
		}
		
		IMAGE_BASE_RELOCATION block{};

		MemoryStatus status = process_.memory().read(
			directoryAddress + offset,
			&block,
			sizeof(block)
		);

		if (status != MemoryStatus::Success) {
			return LoaderStatus::ImageRelocationFailed;
		}

		if (block.SizeOfBlock < sizeof(block) || block.SizeOfBlock > remaining) {
			return LoaderStatus::ImageRelocationFailed;
		}

		uint64_t len = block.SizeOfBlock - sizeof(block);

		// # of entries not being multiple of 16 bits indicates malformed block
		if (len % sizeof(WORD) != 0) {
			return LoaderStatus::ImageRelocationFailed;
		}

		DWORD count = len / sizeof(WORD);

		for (size_t i = 0; i < count; i++) {
			
			WORD entry{};

			status = process_.memory().read(
				directoryAddress + offset + sizeof(IMAGE_BASE_RELOCATION) + i * sizeof(WORD),
				&entry,
				sizeof(entry)
			);

			if (status != MemoryStatus::Success) return LoaderStatus::ImageRelocationFailed;

			WORD type = entry >> 12;
			WORD entryOffset = entry & 0x0FFF;

			if (type == win::IMAGE_REL_BASED_ABSOLUTE) {
				continue;
			}

			if (type != win::IMAGE_REL_BASED_DIR64) { // only relocation that 64 bit PEs use (aside from padding) 
				return LoaderStatus::UnsupportedRelocationType;
			}

			ptr_t patchRva = uint64_t(block.VirtualAddress) + entryOffset;

			if (!validImageRange(patchRva, sizeof(uint64_t), module.sizeOfImage)) {
				return LoaderStatus::ImageRelocationFailed;
			}

			ptr_t patchAddr = module.baseAddress + patchRva;
			uint64_t fix{};

			// Read fix = (guest)*patchAddr
			status = process_.memory().read(
				patchAddr,
				&fix,
				sizeof(fix)
			);

			if (status != MemoryStatus::Success) {
				return LoaderStatus::ImageRelocationFailed;
			}

			// apply patch
			fix += delta;

			// Write back patch
			status = process_.memory().write(
				patchAddr,
				&fix,
				sizeof(fix)
			);

			if (status != MemoryStatus::Success) {
				return LoaderStatus::ImageRelocationFailed;
			}

		}

		offset += block.SizeOfBlock;

	}

	std::cout
		<< "[RELOC] " << module.name
		<< " 0x" << std::hex << preferredBase
		<< " -> 0x" << module.baseAddress
		<< std::dec << '\n';

	return LoaderStatus::Success;
}

LoaderStatus Loader::resolveImageImports(const PEFile& pe, Module& currentModule) {
	
	IMAGE_DATA_DIRECTORY importDirectory = pe.ntHeaders().OptionalHeader.DataDirectory[win::IMAGE_DIRECTORY_ENTRY_IMPORT];

	if (importDirectory.VirtualAddress == 0) return LoaderStatus::Success;

	// Guest address of current import descriptor - *src for temp import
	ptr_t pImport = currentModule.baseAddress + importDirectory.VirtualAddress;
	ptr_t end = pImport + importDirectory.Size;


	while (pImport + sizeof(IMAGE_IMPORT_DESCRIPTOR)<= end) {

		IMAGE_IMPORT_DESCRIPTOR currentImport;

		MemoryStatus status = process_.memory().read(
			pImport,
			&currentImport,
			sizeof(IMAGE_IMPORT_DESCRIPTOR)
		);

		if (status != MemoryStatus::Success) {
			return LoaderStatus::ImportDescriptorError;
		}

		// indicates end of IDT
		if (currentImport.Name == 0) {
			break;
		}
		
		ptr_t importNameRva = currentModule.baseAddress + currentImport.Name;
		std::string importName;

		status = process_.memory().readString(
			importNameRva,
			importName,
			260
		);

		if (status != MemoryStatus::Success) { 
			return LoaderStatus::ImportDescriptorError; 
		}

		// API set contracts : resolve host DLL, update import name
		if (importName.starts_with("ext-ms-") || importName.starts_with("api-ms-")) {

			auto host = apiSetResolver_.resolve(importName, currentModule.name);

			if (!host) {
				return LoaderStatus::ApiSetResolutionFailed;
			}

			importName = *host;
		}

		Module* dependency = process_.modules().findModule(importName);

		if (dependency == nullptr) {

			LoaderStatus imageStatus = loadModule(importName, &dependency);

			if (imageStatus != LoaderStatus::Success) {
				std::cout
					<< "[ERROR] Failed to load dependency\n"
					<< "        image:  " << currentModule.name << '\n'
					<< "        module: " << importName << '\n'
					<< "        status: "
					<< loaderStatusString(imageStatus) << '\n';

				return imageStatus;
			}
		}

		if (dependency != &currentModule && find(currentModule.dependencies.begin(), currentModule.dependencies.end(), dependency) == currentModule.dependencies.end()) {
			currentModule.dependencies.push_back(dependency);
		}

		ptr_t hmodule = dependency->baseAddress;

		// Use IAT for function name/ordinal lookup when OriginalFirstThunk isnt present
		DWORD lookupRva = (currentImport.OriginalFirstThunk != 0) ? currentImport.OriginalFirstThunk : currentImport.FirstThunk;
		ptr_t intAddress = currentModule.baseAddress + lookupRva;
		ptr_t iatAddress = currentModule.baseAddress + currentImport.FirstThunk;
		
		while (true) {

			IMAGE_THUNK_DATA64 thunk;
			
			status = process_.memory().read(
				intAddress,
				&thunk,
				sizeof(thunk)
			);

			if (status != MemoryStatus::Success) { 
				std::cout
					<< "[ERROR] Failed to read import thunk\n"
					<< "        image:  " << currentModule.name << '\n'
					<< "        module: " << importName << '\n'
					<< "        thunk:  0x"
					<< std::hex << intAddress
					<< std::dec << '\n';

				return LoaderStatus::ImportThunkReadFailed;
			}

			if (thunk.u1.AddressOfData == 0) {
				break;
			}

			ptr_t resolvedAddress = 0;
			
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
						<< "        image:   " << currentModule.name << '\n'
						<< "        module:  " << importName << '\n'
						<< "        ordinal: " << ordinal << '\n'
						<< "        status:  "
						<< loaderStatusString(ldrStatus) << '\n';
					
					
					return ldrStatus;
				
				}
			}
			else {
				
				ptr_t importByNameAddr = currentModule.baseAddress + thunk.u1.AddressOfData;
				std::string functionName;

				status = process_.memory().readString(
					importByNameAddr + sizeof(WORD),
					functionName,
					260
				);

				if (status != MemoryStatus::Success) {
					std::cout
						<< "[ERROR] Failed to read import name\n"
						<< "        image:   " << currentModule.name << '\n'
						<< "        module:  " << importName << '\n'
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
						<< "        image:    " << currentModule.name << '\n'
						<< "        module:   " << importName << '\n'
						<< "        function: " << functionName << '\n'
						<< "        status:   "
						<< loaderStatusString(ldrStatus) << '\n';

					return ldrStatus;
				}
			}

			status = process_.memory().write(
				iatAddress,
				&resolvedAddress,
				sizeof(resolvedAddress)
			);

			if (status != MemoryStatus::Success) {
				std::cout
					<< "[ERROR] Failed to write IAT entry\n"
					<< "        image:   " << currentModule.name << '\n'
					<< "        module:  " << importName << '\n'
					<< "        address: 0x"
					<< std::hex << iatAddress
					<< std::dec << '\n';

				return LoaderStatus::IatWriteFailed;
			}

			intAddress += sizeof(IMAGE_THUNK_DATA64);
			iatAddress += sizeof(IMAGE_THUNK_DATA64);
		}

		pImport += sizeof(IMAGE_IMPORT_DESCRIPTOR);
	}

	
	return LoaderStatus::Success;
}



//LoaderStatus Loader::loadExecutable(std::filesystem::path path) {
//
//	PEFile image(path);
//
//	ImageContext* ctx = process_.addImage(path.filename().string());
//	if (ctx == nullptr) {
//		return LoaderStatus::InvalidImageContext;
//	}
//
//	ctx->type = ImageType::Executable;
//	ctx->path = path;
//
//	process_.setMainImage(ctx);
//
//	return loadImage(image, ctx);
//}
//
LoaderStatus Loader::loadModule(const std::string& name, Module** moduleOut) {

	if (moduleOut == nullptr) {
		return LoaderStatus::InvalidImageContext;
	}
	
	Module* module = process_.modules().findModule(name);

	if (module != nullptr) {
		*moduleOut = module;
		return LoaderStatus::Success;
	}

	const auto pathOpt = findImagePath(name);
	if (!pathOpt) {
		std::cout
			<< "[ERROR] Failed to locate module\n"
			<< "        module: " << name << '\n';

		return LoaderStatus::FailedToLocateImage;
	}

	auto peOpt = PEFile::open(*pathOpt);
	if (!peOpt) {
		std::cout << "[ERROR] Failed to open module: " << name << '\n';
		return LoaderStatus::InvalidImage;
	}

	// Setup new module to be loaded
	module = process_.modules().addModule(name);
	module->type = ImageType::DLL;
	module->path = *pathOpt;

	LoaderStatus status = loadImage(*peOpt, *module);
	if (status != LoaderStatus::Success) {
		return status;
	}

	// Wait for module to finish loading & write back new Module pointer
	*moduleOut = module;

	return LoaderStatus::Success;
}

std::optional<std::filesystem::path> Loader::findImagePath(const std::string& name) {

	static const std::filesystem::path systemDirectory = "guest/windows/system32";

	std::filesystem::path candidate = systemDirectory / name;

	if (std::filesystem::is_regular_file(candidate)) {
		return candidate;
	}

	return std::nullopt;

}

LoaderStatus Loader::getExportContext(uint64_t hmodule, IMAGE_DATA_DIRECTORY* out) {
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

	IMAGE_NT_HEADERS nt;

	status = process_.memory().read(
		hmodule + dos.e_lfanew,
		&nt,
		sizeof(nt)
	);

	if (status != MemoryStatus::Success) {
		return LoaderStatus::ExportDirectoryReadFailed;
	}

	*out = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];

	return LoaderStatus::Success;
}

// Resolve by name
LoaderStatus Loader::resolveProcAddress(ptr_t hmodule, const std::string& name, uint64_t* addrOut, size_t depth) {
	
	if (hmodule == 0 || name.empty() || addrOut == nullptr) {
		return LoaderStatus::ExportTableReadFailed;
	}

	IMAGE_DATA_DIRECTORY exportData{};

	LoaderStatus status = getExportContext(hmodule, &exportData);

	if (status != LoaderStatus::Success) {
		return status;
	}

	ptr_t exportAddress = hmodule + exportData.VirtualAddress;
	IMAGE_EXPORT_DIRECTORY exportDir{};

	MemoryStatus memstatus = process_.memory().read(exportAddress, &exportDir, sizeof(exportDir));
	if (memstatus != MemoryStatus::Success) return LoaderStatus::ExportDirectoryReadFailed; 

	ptr_t names = hmodule + exportDir.AddressOfNames;
	ptr_t functions = hmodule + exportDir.AddressOfFunctions;
	ptr_t ordinals = hmodule + exportDir.AddressOfNameOrdinals;

	for (DWORD i = 0; i < exportDir.NumberOfNames; ++i) {

		DWORD nameRva = 0;

		// Read name RVA
		memstatus = process_.memory().read(names + sizeof(DWORD) * i, &nameRva, sizeof(nameRva));

		if (memstatus != MemoryStatus::Success) {
			return LoaderStatus::ExportTableReadFailed;
		}

		std::string functionName;

		// Read function name
		memstatus = process_.memory().readString(hmodule + nameRva, functionName, 260);

		if (memstatus != MemoryStatus::Success) {
			return LoaderStatus::ExportTableReadFailed;
		}

		// Compare current entry function name to target function name
		if (functionName != name) {
			continue;
		}

		WORD functionIndex;

		// On name match, read ordinals[i] for function index
		memstatus = process_.memory().read(ordinals + sizeof(WORD) * i, &functionIndex, sizeof(functionIndex)
		);

		if (memstatus != MemoryStatus::Success) {
			return LoaderStatus::ExportTableReadFailed;
		}

		if (functionIndex >= exportDir.NumberOfFunctions) {
			return LoaderStatus::ExportNotFound;
		}

		DWORD functionRva;

		// Functions[ordinals[i]] = function address OR forwarder
		memstatus = process_.memory().read(functions + sizeof(DWORD) * functionIndex, &functionRva, sizeof(functionRva));

		if (memstatus != MemoryStatus::Success) {
			return LoaderStatus::ExportTableReadFailed;
		}

		return resolveExportRva(
			hmodule,
			functionRva,
			exportData.VirtualAddress,
			exportData.Size,
			addrOut,
			depth
		);
	}

	return LoaderStatus::ExportNotFound;
}

// Resolve by ordinal
LoaderStatus Loader::resolveProcAddress(ptr_t hmodule, WORD ordinal, uint64_t* addrOut, size_t depth) {
	
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
		hmodule + dir.AddressOfFunctions + sizeof(DWORD) * functionIndex,
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

LoaderStatus Loader::resolveForwarder(const std::string& forwarder, ptr_t parentModuleBase, uint64_t* addrOut, size_t depth) {
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

	Module* parentModule = process_.modules().findModuleByBase(parentModuleBase);

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

LoaderStatus Loader::setSectionProtections(const PEFile& pe, Module& module) {

	//for (auto it : process_.getLoadOrder()) {
	//
	//	uint64_t base = it->baseAddress;
	//	
	//	IMAGE_DOS_HEADER dos{};
	//	MemoryStatus status = process_.memory().read(base, &dos, sizeof(IMAGE_DOS_HEADER));
	//	if (status != MemoryStatus::Success) return LoaderStatus::SectionProtectionUpdateFailed;

	//	IMAGE_NT_HEADERS nt{};
	//	status = process_.memory().read(dos.e_lfanew, &nt, sizeof(IMAGE_NT_HEADERS));
	//	if (status != MemoryStatus::Success) return LoaderStatus::SectionProtectionUpdateFailed;

	//	




	//}


	return LoaderStatus::Success;
}


}