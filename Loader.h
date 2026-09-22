// Loader.h
#pragma once

#include "PEFile.h"
#include "Process.h"
#include <string.h>

#include "ImageContext.h"

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
    FailedToLocateImage,

    ApiSetResolutionFailed,
    ExportNotFound,

    Other
};


class Loader {
public:
	explicit Loader(Process& process) : process_(process) {};

    LoaderStatus mapImage(PEFile& pe, ImageContext* ctx);
    LoaderStatus relocateImage(ImageContext* ctx);
    LoaderStatus resolveImageImports(ImageContext* ctx);
    std::optional<std::filesystem::path> findImagePath(const std::string& name);

    LoaderStatus loadImage(PEFile& pe, ImageContext* ctx);
    LoaderStatus loadExecutable(std::filesystem::path path);
    LoaderStatus loadModule(const std::string& name, ImageContext** ctxOut);

    LoaderStatus resolveProcAddress(uint64_t hmodule, std::string name, uint64_t* addrOut);
    

private:
    Process& process_; 
};



