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

enum class InitializationState {
	Uninitialized,
	Initializing, 
	Initialized
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
	InitializationState initState = InitializationState::Uninitialized;
	std::vector<ImageContext*> dependencies;
	uint64_t ldrEntryAddress = 0;
};