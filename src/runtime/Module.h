#pragma once

#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <string>

#include "common/Types.h"

namespace emu {

    enum class ModuleState {
        Loading,
        Loaded,
        Failed
    };

    enum class ImageType {
        Executable,
        DLL
    };

    // Eventually need to add reference counts here
    struct Module {
        std::string name;
        std::filesystem::path path;

        ptr_t baseAddress = 0;
        ptr_t preferredBase = 0;
        ptr_t entryPoint = 0;
        size_t sizeOfImage = 0;

        ImageType type = ImageType::DLL;
        ModuleState state = ModuleState::Loading;
    };
}; 