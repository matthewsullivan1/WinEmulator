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

private:
	GuestMemory memory_;
	MemoryManager memoryManager_;
	ImageContext* mainImage_ = nullptr;
	std::unordered_map<std::string, ImageContext> images_;
};

