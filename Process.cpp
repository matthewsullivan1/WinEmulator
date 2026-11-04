// Process.cpp

#include "Process.h"
#include "WindowsStructs.h"
#include <iostream>

ImageContext* Process::findImage(std::string name) {
	
	std::string original = name;
	name = normalizeModuleName(std::move(name));

	//std::cout
	//	<< "[LOOKUP] \"" << original
	//	<< "\" -> \"" << name << "\"\n";

	auto it = images_.find(name);

	//std::cout
	//	<< "         "
	//	<< (it == images_.end() ? "NOT FOUND" : "FOUND")
	//	<< '\n';

	if (it == images_.end()) {
		return nullptr;
	}

	return &it->second;
}

const ImageContext* Process::findImage(std::string name) const {

	std::string original = name;
	name = normalizeModuleName(std::move(name));

	//std::cout
	//	<< "[LOOKUP] \"" << original
	//	<< "\" -> \"" << name << "\"\n";

	const auto it = images_.find(name);

	//std::cout
	//	<< "         "
	//	<< (it == images_.end() ? "NOT FOUND" : "FOUND")
	//	<< '\n';

	if (it == images_.end()) {
		return nullptr;
	}

	return &it->second;
}

ImageContext* Process::addImage(std::string name) {

	std::string original = name;
	name = normalizeModuleName(std::move(name));

	//std::cout
	//	<< "[ADD] \"" << original
	//	<< "\" -> \"" << name << "\"\n";

	auto [it, inserted] = images_.try_emplace(
		name,
		ImageContext{
			.name = name,
		}
		);

	if (inserted) {
		loadOrder_.push_back(&it->second);
	}

	return &it->second;
}

void Process::setMainImage(ImageContext* ctx) {
	mainImage_ = ctx;
}

std::string Process::normalizeModuleName(std::string name) {
	std::transform(
		name.begin(),
		name.end(),
		name.begin(),
		[](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		}
	);

	return name;
}


ImageContext* Process::findImageByBase(uint64_t baseAddress) {

	for (auto& [name, image] : images_) {

		if (image.baseAddress == baseAddress) {
			return &image;
		}
	}

	return nullptr;
}

const ImageContext* Process::findImageByBase(uint64_t baseAddress) const {

	for (const auto& [name, image] : images_) {

		if (image.baseAddress == baseAddress) {
			return &image;
		}
	}

	return nullptr;
}

MemoryStatus Process::initializePeb() {
	
	uint64_t pebAddress;
	uint64_t ldrAddress;

	_GUEST_PEB peb;
	_GUEST_PEB_LDR_DATA ldr;

	MemoryStatus status = memoryManager_.allocate(0, sizeof(_GUEST_PEB), PageProtection::ReadWrite, RegionType::Private, &pebAddress);

	if (status != MemoryStatus::Success) {
		return status;
	}

	status = memoryManager_.allocate(0, sizeof(_GUEST_PEB_LDR_DATA), PageProtection::ReadWrite, RegionType::Private, &ldrAddress);

	if (status != MemoryStatus::Success) {
		return status;
	}

	pebAddress_ = pebAddress;
	peb.ImageBaseAddress = mainImage_->baseAddress;
	peb.Ldr = ldrAddress;

	ldr.Length = sizeof(_GUEST_PEB_LDR_DATA);

	for (auto& ctx : loadOrder_) {

		uint64_t entryAddress;
		_GUEST_LDR_DATA_TABLE_ENTRY entry; 

		MemoryStatus status = memoryManager_.allocate(
			0, 
			sizeof(_GUEST_LDR_DATA_TABLE_ENTRY),
			PageProtection::ReadWrite,
			RegionType::Private, 
			&entryAddress
		);

		if (status != MemoryStatus::Success) {
			return status;
		}

		ctx->ldrEntryAddress = entryAddress;
		
		entry.DllBase = ctx->baseAddress;
		entry.EntryPoint = ctx->entryPoint;
		entry.SizeOfImage = ctx->size;

		std::wstring modulePath = ctx->path.wstring();
		std::wstring moduleName(ctx->name.begin(), ctx->name.end());

		_GUEST_UNICODE_STRING FullDllName;
		_GUEST_UNICODE_STRING BaseDllName;

		FullDllName.Length = static_cast<uint16_t>(sizeof(wchar_t) * modulePath.size());
		BaseDllName.Length = static_cast<uint16_t>(sizeof(wchar_t) * moduleName.size());

		FullDllName.MaximumLength = static_cast<uint16_t>(sizeof(wchar_t) * (modulePath.size() + 1));
		BaseDllName.MaximumLength = static_cast<uint16_t>(sizeof(wchar_t) * (moduleName.size() + 1));

		uint64_t pathBuf, nameBuf;

		status = memoryManager_.allocate(0, static_cast<size_t>(FullDllName.MaximumLength), PageProtection::ReadWrite, RegionType::Private, &pathBuf);
		if (status != MemoryStatus::Success) { return status; }

		status = memoryManager_.allocate(0, static_cast<size_t>(BaseDllName.MaximumLength), PageProtection::ReadWrite, RegionType::Private, &nameBuf);
		if (status != MemoryStatus::Success) { return status; }

		FullDllName.Buffer = pathBuf;
		BaseDllName.Buffer = nameBuf;

		status = memoryManager_.write(pathBuf, modulePath.c_str(), FullDllName.MaximumLength);
		if (status != MemoryStatus::Success) { return status; }

		status = memoryManager_.write(nameBuf, moduleName.c_str(), BaseDllName.MaximumLength);
		if (status != MemoryStatus::Success) { return status; }

		entry.FullDllName = FullDllName;
		entry.BaseDllName = BaseDllName;

		status = memoryManager_.write(entryAddress, &entry, sizeof(entry));
		if (status != MemoryStatus::Success) { return status; }

		printf("FullDllName:\t%ls\nBaseDllName:\t%ls\nVirtualAddress:\t%llu\n\n", modulePath.c_str(), moduleName.c_str(), entryAddress);

	}

	return MemoryStatus::Success;

}

MemoryStatus Process::linkModulesList() {

	_GUEST_PEB peb{};
	_GUEST_PEB_LDR_DATA ldr{};
	MemoryStatus status; 

	status = memoryManager_.read(pebAddress_, &peb, sizeof(_GUEST_PEB));
	if (status != MemoryStatus::Success) { return status; }

	status = memoryManager_.read(peb.Ldr, &ldr, sizeof(_GUEST_PEB_LDR_DATA));
	if (status != MemoryStatus::Success) { return status; }

	/*
	Loop over the loadOrder image context pointers, get their ldr data table entry VA, read, 
	
	*/


}