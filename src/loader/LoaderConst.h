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
    SectionProtectionUpdateFailed,
    SectionReadFailed,

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