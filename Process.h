// Process.h
#pragma once

#include "MemoryManager.h"
#include "GuestMemory.h"
#include "ImageContext.h"

class Process {
public:
	Process() : memoryManager_(memory_) {} 

	MemoryManager& memory() {
		return memoryManager_;
	}

	const MemoryManager& memory() const {
		return memoryManager_;
	}

	ImageContext* findImage(std::string name);

	const ImageContext* findImage(std::string name) const;
	
	ImageContext* addImage(std::string name);

	void setMainImage(ImageContext* ctx);

	ImageContext* mainImage() { return mainImage_; }

	ImageContext* findImageByBase(uint64_t baseAddress);
	const ImageContext* findImageByBase(uint64_t baseAddress) const;

	MemoryStatus initializePeb();
	MemoryStatus linkModulesList();
	const uint64_t getPeb() { return pebAddress_; }

	const std::vector<ImageContext*>& getLoadOrder() const { return loadOrder_; }



private:
	GuestMemory memory_;
	MemoryManager memoryManager_;

	ImageContext* mainImage_ = nullptr;
	std::unordered_map<std::string, ImageContext> images_;
	std::vector<ImageContext*> loadOrder_;

	uint64_t pebAddress_ = 0;
	uint64_t tebAddress_ = 0;
	

	static std::string normalizeModuleName(std::string name);
};

