// Process.cpp

#include "Process.h"
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
	MemoryStatus status = memoryManager_.allocate(0, sizeof(PEB), PageProtection::ReadWrite, RegionType::Private, &pebAddress);

	if (status != MemoryStatus::Success) {
		return status;
	}





}
