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
    ImageSectionProtectFailed,
    ImageReservationFailed,
    ImageHeaderMapFailed,
    ImageSectionMapFailed,
    InvalidSectionAlignment,
    UnsupportedSectionAlignment,
    ImageHeaderCommitFailed,
    ImageSectionCommitFailed,
    ImageRelocationFailed,
    MalformedBaseRelocationDirectory,
    UnsupportedRelocationType,
    ImportDescriptorError,
    FailedToLocateImage,
    ApiSetResolutionFailed,
    ExportNotFound,
    ForwarderError,
    InvalidModuleContext,
    ImportThunkReadFailed,
    ImportNameReadFailed,
    IatWriteFailed,
    ExportDirectoryReadFailed,
    ExportTableReadFailed,
    ForwarderReadFailed,
    InvalidArgument,
    ForwarderDepthExceeded,
    InvalidForwarder,

    Other
};

inline const char* loaderStatusString(LoaderStatus status)
{
    switch (status) {
    case LoaderStatus::Success:
        return "Success";

    case LoaderStatus::InvalidImage:
        return "InvalidImage";

    case LoaderStatus::InvalidImageContext:
        return "InvalidImageContext";

    case LoaderStatus::ImageSectionProtectFailed:
        return "ImageSectionProtectFailed";

    case LoaderStatus::ImageReservationFailed:
        return "ImageReservationFailed";

    case LoaderStatus::ImageHeaderCommitFailed:
        return "ImageHeaderCommitFailed";

    case LoaderStatus::ImageHeaderMapFailed:
        return "ImageHeaderMapFailed";

    case LoaderStatus::InvalidSectionAlignment:
        return "InvalidSectionAlignment";

    case LoaderStatus::ImageSectionCommitFailed:
        return "ImageSectionCommitFailed";

    case LoaderStatus::ImageSectionMapFailed:
        return "ImageSectionMapFailed";

    case LoaderStatus::UnsupportedSectionAlignment:
        return "UnsupportedSectionAlignment";

    case LoaderStatus::ImageRelocationFailed:
        return "ImageRelocationFailed";

    case LoaderStatus::UnsupportedRelocationType:
        return "UnsupportedRelocationType";

    case LoaderStatus::ImportDescriptorError:
        return "ImportDescriptorError";

    case LoaderStatus::ApiSetResolutionFailed:
        return "ApiSetResolutionFailed";

    case LoaderStatus::FailedToLocateImage:
        return "FailedToLocateImage";

    case LoaderStatus::ExportNotFound:
        return "ExportNotFound";

    case LoaderStatus::ForwarderError:
        return "ForwarderError";
    
    case LoaderStatus::InvalidModuleContext:
        return "InvalidModuleContext";

    case LoaderStatus::ImportThunkReadFailed:
        return "ImportThunkReadFailed";

    case LoaderStatus::ImportNameReadFailed:
        return "ImportNameReadFailed";

    case LoaderStatus::IatWriteFailed:
        return "IatWriteFailed";

    case LoaderStatus::ExportDirectoryReadFailed:
        return "ExportDirectoryReadFailed";

    case LoaderStatus::ExportTableReadFailed:
        return "ExportTableReadFailed";
        
    case LoaderStatus::ForwarderReadFailed:
        return "ForwarderReadFailed";
        
    case LoaderStatus::InvalidArgument:
        return "InvalidArgument";

    case LoaderStatus::ForwarderDepthExceeded:
        return "ForwarderDepthExceeded";

    case LoaderStatus::InvalidForwarder:
        return "InvalidForwarder";

    case LoaderStatus::Other:
        return "Other";


    default:
        return "Unknown";
    }
}

struct ExportContext {
    DWORD directoryRva = 0;
    DWORD directorySize = 0;
    IMAGE_EXPORT_DIRECTORY directory;
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
    LoaderStatus getExportContext(uint64_t hmodule, ExportContext& out);

    LoaderStatus resolveProcAddress(uint64_t hmodule, const std::string& name, uint64_t* addrOut, size_t depth = 0);
    LoaderStatus resolveProcAddress(uint64_t hmodule, WORD ordinal, uint64_t* addrOut, size_t depth = 0);
    LoaderStatus resolveForwarder(const std::string& forwarder, uint64_t parentModuleBase, uint64_t* addrOut, size_t depth = 0);
    LoaderStatus resolveExportRva(uint64_t hmodule, DWORD functionRva, DWORD exportDirRva, DWORD exportDirSize, uint64_t* addrOut, size_t depth = 0);

    LoaderStatus setSectionProtections();

private:
    Process& process_; 
};


/*
Loader steps from windows internals

1. ckec LdrpProcessInitialized (NA)
2. Set LdrInitState to 0, set PEB->ProcessInitializing to 1, TEB->RanProcessInit to 1
3. init loader lock (?)
4. init dynamic function table, for unwind/exception support (?)
5. init mutable RO heap section (MRDATA) for security relevant global variables (?)
y 6. init loader database in PEB (is this inordermoduleslist?)
7. init NLS (NA)
8. builds image path name for application
9. captures SEH exception handlers from .pdata section and builds the internal exception tables
10. captures system call thunks for the five critical loader functions: ntcreatesection, ntopenfile, ntqueryattributes, ntopensection, ntmapviewofsection
11. reads the mitigation options for the application, passed by the kernel 
12. queries IFEO registry key for the applicaiton 
13. loads configuration options specified in image load config directory 
15. init FLS and TLS
16. sets up debug options for critical sections, creates user mode stack trace databaser if enabled, queries stacktracedatabaseinsizemb
17. initializes heap manager for process and creates the first process heap. uses various load config, image file excution, global flags, and header options 
18. enables terminate process on heap corruption if enabled
19. init exception dispatch log if needed 
20. init thread pool package
21. init and converts environment bloick and parameter block as needed to support wow64 
22. opens knowndlls and builds known dll path
23. for store applications, reads application model policy options
24. determines the processes current directory, system path, default load path, as well as rules around dll search order. 
y 25. builds the first loader data table entry for ntdll.dll and inserts it into the module database 
26. builds unwind history table
27. init parallel loader, which is used to load all dependencies that dont have cross dependencies using the thread pool and concurrent threads
y 28. builds the next loader data table entry for the main executable and inserts it into the module database
29. relocates main executable image
30. if enabled, init application verifier 
31. init wow64 engine if needed
32. .NET image handling 
33. initializes TLS slots of the process 
34. manually load kernel32.dll and kernelbase.dll regardless of the actual imports of the process. as needed, it uses these libs to init SRP mechanisms, as well as capture the windows subsystem thread initialization thunk function. finally, resolves any API set dependencies that exist specifically between these two libs
35. init shim engine and parses shim database 
36. enables parallel image loader, as long as the core loader functions scanned earlier do not have any system call hooks or detours attached to them, and based on the number of loader threads that have neen configured through image file execution options
37. sets ldrinitstate to 1, meaning import loading is in progress 

at this point, the image loader is ready to start parsing the IAT
dependency map can be created prior to this, such that DLLs can be loaded in parallel for DLLs which dont share dependencies





*/
