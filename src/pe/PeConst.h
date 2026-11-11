
enum class PEError {
    FileNotFound,
    FileOpenFailed,
    FileReadFailed,
    EmptyFile,
    InvalidDOSHeader,
    InvalidNTHeader,
    InvalidOptionalHeader,
    InvalidSectionTable,
    UnsupportedArchitecture
};