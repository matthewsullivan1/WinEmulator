// Loader.h
#pragma once

#include "PEFile.h"
#include "Process.h"
#include <string.h>

#include "Module.h"

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
    MalformedBaseRelocationDirectory,
    UnsupportedRelocationType,

    ImportDescriptorError,


    Other
};

struct ImageContext {
    uint64_t baseAddress = 0;
    uint64_t preferredBase = 0;
    uint64_t entryPoint = 0;
    size_t size = 0;

    ImageType type = ImageType::Executable;
};


class Loader {
public:
	explicit Loader(Process& process) : process_(process) {};

    LoaderStatus mapImage(PEFile& pe, ImageContext* ctx);
    LoaderStatus relocateImage(ImageContext* ctx);
    LoaderStatus resolveImageImports(ImageContext* ctx);

private:
    Process& process_; 
};



