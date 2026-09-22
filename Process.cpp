// Process.cpp

#include "Process.h"

ImageContext* Process::findImage(std::string name) {
	
	auto it = images_.find(name);

	if (it == images_.end()) {
		return nullptr;
	}

	return &it->second;
}

const ImageContext* Process::findImage(std::string name) const {

	const auto it = images_.find(name);

	if (it == images_.end()) {
		return nullptr;
	}

	return &it->second;
}

ImageContext* Process::addImage(std::string name) {

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

