// Loader.h
#pragma once

#include "MemoryManager.h"
#include "PEFile.h"
#include <string.h>

enum class ImageType {
    Executable,
    DLL
};

struct ImageContext {
    uint64_t baseAddress = 0;
    uint64_t preferredBase = 0;
    uint64_t entryPoint = 0;
    size_t size = 0;

    ImageType type = ImageType::Executable;
};

enum class LoaderStatus {
    Success,

    InvalidImage,
    InvalidImageContext,

    ImageReservationFailed,
    
    ImageHeaderMapFailed,
    ImageSectionMapFailed,

    ImageHeaderCommitFailed,
    ImageSectionCommitFailed,

    ImageRelocationFailed,

    
    Other
};


class Loader {
public:
	explicit Loader(MemoryManager& mm) : mm_(mm) {};

    LoaderStatus mapImage(PEFile& pe, ImageContext* ctx);
    LoaderStatus relocateImage(ImageContext* ctx);
    LoaderStatus resolveImageImports(ImageContext* ctx);

private:
	MemoryManager& mm_;
};



