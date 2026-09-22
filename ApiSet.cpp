#include "ApiSet.h"

#include <Windows.h>
#include <winternl.h>
#include <intrin.h>
#include <algorithm>
#include <cwctype>
#include <iostream>
#include <filesystem>

// Private to ApiSet.cpp
namespace
{
    struct ApiSetNamespace
    {
        ULONG Version;
        ULONG Size;
        ULONG Flags;
        ULONG Count;
        ULONG EntryOffset;
        ULONG HashOffset;
        ULONG HashFactor;
    };

    struct ApiSetNamespaceEntry
    {
        ULONG Flags;
        ULONG NameOffset;
        ULONG NameLength;
        ULONG HashedLength;
        ULONG ValueOffset;
        ULONG ValueCount;
    };

    struct ApiSetValueEntry
    {
        ULONG Flags;
        ULONG NameOffset;
        ULONG NameLength;
        ULONG ValueOffset;
        ULONG ValueLength;
    };
}


// Public ApiSet functions

bool ApiSet::isContract(std::string_view name)
{
    return name.starts_with("api-") ||
        name.starts_with("ext-");
}


std::optional<std::wstring> ApiSet::resolve(std::wstring_view contract)
{
    std::wstring name(contract);

    HMODULE module = LoadLibraryW(name.c_str());

    if (!module) {
        return std::nullopt;
    }

    wchar_t path[MAX_PATH]{};

    DWORD length = GetModuleFileNameW(
        module,
        path,
        MAX_PATH
    );

    if (length == 0 || length == MAX_PATH)
    {
        FreeLibrary(module);
        return std::nullopt;
    }

    FreeLibrary(module);

    std::filesystem::path modulePath(path);

    return modulePath.filename().wstring();
}