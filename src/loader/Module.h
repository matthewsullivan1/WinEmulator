#pragma once

#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <string>

enum class ModuleState {
    Loading,
    Loaded,
    Failed
};

enum class ImageType {
    Executable,
    DLL
};

struct Module {
    std::string name;
    std::filesystem::path path;

    uint64_t baseAddress = 0;
    uint64_t entryPoint = 0;
    size_t sizeOfImage = 0;

    ImageType type = ImageType::Executable;
    ModuleState state = ModuleState::Loading;
};
