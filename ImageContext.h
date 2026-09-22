#pragma once

#include <string>
#include <filesystem>
#include <cstdint>

enum class ImageType {
	Executable,
	DLL
};

enum class ImageState {
	Loading,
	Loaded,
	Failed
};

struct ImageContext {
	std::string name;
	std::filesystem::path path;

	uint64_t baseAddress = 0;
	uint64_t preferredBase = 0;
	uint64_t entryPoint = 0;
	size_t size = 0;

	ImageType type;
	ImageState state = ImageState::Loading; 
};