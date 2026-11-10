#pragma once

#ifdef _WIN32

#include <Windows.h>

#else

#include <cstdint>
#include <cstddef>

using BYTE =		std::uint8_t;
using WORD =		std::uint16_t;
using DWORD =		std::uint32_t;
using LONG =		std::int32_t;
using ULONGLONG =	std::uint64_t;


#define IMAGE_DOS_SIGNATURE							0x5A4D
#define IMAGE_NT_SIGNATURE							0x00004550
#define IMAGE_NUMBEROF_DATADIRECTORY_ENTRIES		16
#define IMAGE_SIZEOF_SHORT_NAME						8

#define IMAGE_FILE_MACHINE_UNKNOWN					0x0000
#define IMAGE_FILE_MACHINE_I386						0x014C
#define IMAGE_FILE_MACHINE_AMD64					0x8664
#define IMAGE_FILE_MACHINE_ARM64					0xAA64

#define IMAGE_FILE_RELOCS_STRIPPED          0x0001
#define IMAGE_FILE_EXECUTABLE_IMAGE         0x0002
#define IMAGE_FILE_LINE_NUMS_STRIPPED       0x0004
#define IMAGE_FILE_LOCAL_SYMS_STRIPPED      0x0008
#define IMAGE_FILE_LARGE_ADDRESS_AWARE      0x0020
#define IMAGE_FILE_32BIT_MACHINE            0x0100
#define IMAGE_FILE_DEBUG_STRIPPED           0x0200
#define IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP  0x0400
#define IMAGE_FILE_NET_RUN_FROM_SWAP        0x0800
#define IMAGE_FILE_SYSTEM                   0x1000
#define IMAGE_FILE_DLL                      0x2000
#define IMAGE_FILE_UP_SYSTEM_ONLY           0x4000

#define IMAGE_NT_OPTIONAL_HDR32_MAGIC       0x10b
#define IMAGE_NT_OPTIONAL_HDR64_MAGIC       0x20b
#define IMAGE_ROM_OPTIONAL_HDR_MAGIC        0x107

#define IMAGE_SUBSYSTEM_UNKNOWN                     0
#define IMAGE_SUBSYSTEM_NATIVE                      1
#define IMAGE_SUBSYSTEM_WINDOWS_GUI                 2
#define IMAGE_SUBSYSTEM_WINDOWS_CUI                 3
#define IMAGE_SUBSYSTEM_OS2_CUI                     5
#define IMAGE_SUBSYSTEM_POSIX_CUI                   7
#define IMAGE_SUBSYSTEM_NATIVE_WINDOWS              8
#define IMAGE_SUBSYSTEM_WINDOWS_CE_GUI              9
#define IMAGE_SUBSYSTEM_EFI_APPLICATION             10
#define IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER     11
#define IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER          12
#define IMAGE_SUBSYSTEM_EFI_ROM                     13
#define IMAGE_SUBSYSTEM_XBOX                        14
#define IMAGE_SUBSYSTEM_WINDOWS_BOOT_APPLICATION    16

#define IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA       0x0020
#define IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE          0x0040
#define IMAGE_DLLCHARACTERISTICS_FORCE_INTEGRITY       0x0080
#define IMAGE_DLLCHARACTERISTICS_NX_COMPAT             0x0100
#define IMAGE_DLLCHARACTERISTICS_NO_ISOLATION          0x0200
#define IMAGE_DLLCHARACTERISTICS_NO_SEH                0x0400
#define IMAGE_DLLCHARACTERISTICS_NO_BIND               0x0800
#define IMAGE_DLLCHARACTERISTICS_APPCONTAINER          0x1000
#define IMAGE_DLLCHARACTERISTICS_WDM_DRIVER            0x2000
#define IMAGE_DLLCHARACTERISTICS_GUARD_CF              0x4000
#define IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE 0x8000

#define IMAGE_DIRECTORY_ENTRY_EXPORT          0
#define IMAGE_DIRECTORY_ENTRY_IMPORT          1
#define IMAGE_DIRECTORY_ENTRY_RESOURCE        2
#define IMAGE_DIRECTORY_ENTRY_EXCEPTION       3
#define IMAGE_DIRECTORY_ENTRY_SECURITY        4
#define IMAGE_DIRECTORY_ENTRY_BASERELOC       5
#define IMAGE_DIRECTORY_ENTRY_DEBUG           6
#define IMAGE_DIRECTORY_ENTRY_ARCHITECTURE    7
#define IMAGE_DIRECTORY_ENTRY_GLOBALPTR       8
#define IMAGE_DIRECTORY_ENTRY_TLS             9
#define IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG     10
#define IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT    11
#define IMAGE_DIRECTORY_ENTRY_IAT             12
#define IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT    13
#define IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR  14

#define IMAGE_SCN_TYPE_NO_PAD             0x00000008
#define IMAGE_SCN_CNT_CODE               0x00000020
#define IMAGE_SCN_CNT_INITIALIZED_DATA   0x00000040
#define IMAGE_SCN_CNT_UNINITIALIZED_DATA 0x00000080

#define IMAGE_SCN_LNK_INFO                0x00000200
#define IMAGE_SCN_LNK_REMOVE              0x00000800
#define IMAGE_SCN_LNK_COMDAT              0x00001000

#define IMAGE_SCN_GPREL                   0x00008000

#define IMAGE_SCN_MEM_PURGEABLE           0x00020000
#define IMAGE_SCN_MEM_16BIT               0x00020000
#define IMAGE_SCN_MEM_LOCKED              0x00040000
#define IMAGE_SCN_MEM_PRELOAD             0x00080000

#define IMAGE_SCN_ALIGN_1BYTES            0x00100000
#define IMAGE_SCN_ALIGN_2BYTES            0x00200000
#define IMAGE_SCN_ALIGN_4BYTES            0x00300000
#define IMAGE_SCN_ALIGN_8BYTES            0x00400000
#define IMAGE_SCN_ALIGN_16BYTES           0x00500000
#define IMAGE_SCN_ALIGN_32BYTES           0x00600000
#define IMAGE_SCN_ALIGN_64BYTES           0x00700000
#define IMAGE_SCN_ALIGN_128BYTES          0x00800000
#define IMAGE_SCN_ALIGN_256BYTES          0x00900000
#define IMAGE_SCN_ALIGN_512BYTES          0x00A00000
#define IMAGE_SCN_ALIGN_1024BYTES         0x00B00000
#define IMAGE_SCN_ALIGN_2048BYTES         0x00C00000
#define IMAGE_SCN_ALIGN_4096BYTES         0x00D00000
#define IMAGE_SCN_ALIGN_8192BYTES         0x00E00000

#define IMAGE_SCN_LNK_NRELOC_OVFL         0x01000000
#define IMAGE_SCN_MEM_DISCARDABLE         0x02000000
#define IMAGE_SCN_MEM_NOT_CACHED          0x04000000
#define IMAGE_SCN_MEM_NOT_PAGED           0x08000000
#define IMAGE_SCN_MEM_SHARED              0x10000000
#define IMAGE_SCN_MEM_EXECUTE             0x20000000
#define IMAGE_SCN_MEM_READ                0x40000000
#define IMAGE_SCN_MEM_WRITE               0x80000000

struct IMAGE_DOS_HEADER {
    WORD e_magic;
    WORD e_cblp;
    WORD e_cp;
    WORD e_crlc;
    WORD e_cparhdr;
    WORD e_minalloc;
    WORD e_maxalloc;
    WORD e_ss;
    WORD e_sp;
    WORD e_csum;
    WORD e_ip;
    WORD e_cs;
    WORD e_lfarlc;
    WORD e_ovno;
    WORD e_res[4];
    WORD e_oemid;
    WORD e_oeminfo;
    WORD e_res2[10];
    LONG e_lfanew;
};

struct IMAGE_FILE_HEADER {
    WORD  Machine;
    WORD  NumberOfSections;
    DWORD TimeDateStamp;
    DWORD PointerToSymbolTable;
    DWORD NumberOfSymbols;
    WORD  SizeOfOptionalHeader;
    WORD  Characteristics;
};

struct IMAGE_DATA_DIRECTORY {
    DWORD VirtualAddress;
    DWORD Size;
};

struct IMAGE_OPTIONAL_HEADER64 {
    WORD      Magic;
    BYTE      MajorLinkerVersion;
    BYTE      MinorLinkerVersion;
    DWORD     SizeOfCode;
    DWORD     SizeOfInitializedData;
    DWORD     SizeOfUninitializedData;
    DWORD     AddressOfEntryPoint;
    DWORD     BaseOfCode;
    ULONGLONG ImageBase;
    DWORD     SectionAlignment;
    DWORD     FileAlignment;
    WORD      MajorOperatingSystemVersion;
    WORD      MinorOperatingSystemVersion;
    WORD      MajorImageVersion;
    WORD      MinorImageVersion;
    WORD      MajorSubsystemVersion;
    WORD      MinorSubsystemVersion;
    DWORD     Win32VersionValue;
    DWORD     SizeOfImage;
    DWORD     SizeOfHeaders;
    DWORD     CheckSum;
    WORD      Subsystem;
    WORD      DllCharacteristics;
    ULONGLONG SizeOfStackReserve;
    ULONGLONG SizeOfStackCommit;
    ULONGLONG SizeOfHeapReserve;
    ULONGLONG SizeOfHeapCommit;
    DWORD     LoaderFlags;
    DWORD     NumberOfRvaAndSizes;

    IMAGE_DATA_DIRECTORY
        DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
};

struct IMAGE_NT_HEADERS64 {
    DWORD                   Signature;
    IMAGE_FILE_HEADER       FileHeader;
    IMAGE_OPTIONAL_HEADER64 OptionalHeader;
};

struct IMAGE_SECTION_HEADER {
    BYTE Name[IMAGE_SIZEOF_SHORT_NAME];

    union {
        DWORD PhysicalAddress;
        DWORD VirtualSize;
    } Misc;

    DWORD VirtualAddress;
    DWORD SizeOfRawData;
    DWORD PointerToRawData;
    DWORD PointerToRelocations;
    DWORD PointerToLinenumbers;
    WORD  NumberOfRelocations;
    WORD  NumberOfLinenumbers;
    DWORD Characteristics;
};

struct IMAGE_IMPORT_DESCRIPTOR {
    union {
        DWORD Characteristics;
        DWORD OriginalFirstThunk;
    };

    DWORD TimeDateStamp;
    DWORD ForwarderChain;
    DWORD Name;
    DWORD FirstThunk;
};

struct IMAGE_THUNK_DATA64 {
    union {
        ULONGLONG ForwarderString;
        ULONGLONG Function;
        ULONGLONG Ordinal;
        ULONGLONG AddressOfData;
    } u1;
};

#define IMAGE_ORDINAL_FLAG64 0x8000000000000000ULL

#define IMAGE_SNAP_BY_ORDINAL64(ordinal) \
    (((ordinal) & IMAGE_ORDINAL_FLAG64) != 0)

#define IMAGE_ORDINAL64(ordinal) \
    ((ordinal) & 0xffff)

struct IMAGE_IMPORT_BY_NAME {
    WORD Hint;
    char Name[1];
};

struct IMAGE_EXPORT_DIRECTORY {
    DWORD Characteristics;
    DWORD TimeDateStamp;
    WORD  MajorVersion;
    WORD  MinorVersion;
    DWORD Name;
    DWORD Base;
    DWORD NumberOfFunctions;
    DWORD NumberOfNames;
    DWORD AddressOfFunctions;
    DWORD AddressOfNames;
    DWORD AddressOfNameOrdinals;
};

struct IMAGE_BASE_RELOCATION {
    DWORD VirtualAddress;
    DWORD SizeOfBlock;
};

#define IMAGE_REL_BASED_ABSOLUTE  0
#define IMAGE_REL_BASED_HIGH      1
#define IMAGE_REL_BASED_LOW       2
#define IMAGE_REL_BASED_HIGHLOW   3
#define IMAGE_REL_BASED_HIGHADJ   4
#define IMAGE_REL_BASED_DIR64     10

struct RUNTIME_FUNCTION {
    DWORD BeginAddress;
    DWORD EndAddress;
    DWORD UnwindData;
};

// Windows also typedefs this as IMAGE_RUNTIME_FUNCTION_ENTRY.
using IMAGE_RUNTIME_FUNCTION_ENTRY = RUNTIME_FUNCTION;

struct IMAGE_TLS_DIRECTORY64 {
    ULONGLONG StartAddressOfRawData;
    ULONGLONG EndAddressOfRawData;
    ULONGLONG AddressOfIndex;
    ULONGLONG AddressOfCallBacks;
    DWORD     SizeOfZeroFill;
    DWORD     Characteristics;
};

struct IMAGE_DEBUG_DIRECTORY {
    DWORD Characteristics;
    DWORD TimeDateStamp;
    WORD  MajorVersion;
    WORD  MinorVersion;
    DWORD Type;
    DWORD SizeOfData;
    DWORD AddressOfRawData;
    DWORD PointerToRawData;
};

#define IMAGE_DEBUG_TYPE_UNKNOWN        0
#define IMAGE_DEBUG_TYPE_COFF           1
#define IMAGE_DEBUG_TYPE_CODEVIEW       2
#define IMAGE_DEBUG_TYPE_FPO            3
#define IMAGE_DEBUG_TYPE_MISC           4
#define IMAGE_DEBUG_TYPE_EXCEPTION      5
#define IMAGE_DEBUG_TYPE_FIXUP          6
#define IMAGE_DEBUG_TYPE_OMAP_TO_SRC    7
#define IMAGE_DEBUG_TYPE_OMAP_FROM_SRC  8
#define IMAGE_DEBUG_TYPE_BORLAND        9
#define IMAGE_DEBUG_TYPE_RESERVED10     10
#define IMAGE_DEBUG_TYPE_CLSID          11
#define IMAGE_DEBUG_TYPE_REPRO          16

#define IMAGE_FIRST_SECTION(ntheader)                                      \
    reinterpret_cast<IMAGE_SECTION_HEADER*>(                               \
        reinterpret_cast<std::uintptr_t>(ntheader) +                       \
        offsetof(IMAGE_NT_HEADERS64, OptionalHeader) +                     \
        (ntheader)->FileHeader.SizeOfOptionalHeader)

static_assert(sizeof(BYTE) == 1);
static_assert(sizeof(WORD) == 2);
static_assert(sizeof(DWORD) == 4);
static_assert(sizeof(LONG) == 4);
static_assert(sizeof(ULONGLONG) == 8);

static_assert(sizeof(IMAGE_DOS_HEADER) == 64);
static_assert(offsetof(IMAGE_DOS_HEADER, e_lfanew) == 0x3C);

static_assert(sizeof(IMAGE_FILE_HEADER) == 20);
static_assert(sizeof(IMAGE_DATA_DIRECTORY) == 8);
static_assert(sizeof(IMAGE_OPTIONAL_HEADER64) == 240);
static_assert(sizeof(IMAGE_NT_HEADERS64) == 264);
static_assert(sizeof(IMAGE_SECTION_HEADER) == 40);

static_assert(sizeof(IMAGE_IMPORT_DESCRIPTOR) == 20);
static_assert(sizeof(IMAGE_THUNK_DATA64) == 8);
static_assert(sizeof(IMAGE_EXPORT_DIRECTORY) == 40);
static_assert(sizeof(IMAGE_BASE_RELOCATION) == 8);
static_assert(sizeof(RUNTIME_FUNCTION) == 12);
static_assert(sizeof(IMAGE_TLS_DIRECTORY64) == 40);
static_assert(sizeof(IMAGE_DEBUG_DIRECTORY) == 28);


#endif //_WIN32