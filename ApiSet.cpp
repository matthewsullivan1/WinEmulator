#include "ApiSet.h"

#include <Windows.h>
#include <winternl.h>
#include <intrin.h>
#include <algorithm>
#include <cwctype>
#include <cstddef>
#include <cstdint>
#include <iostream>

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


std::optional<std::wstring> ApiSet::resolve(std::wstring_view contract, std::wstring_view parentModule)
{
    //std::wcout
    //    << L"[APISET-DEBUG] Resolving contract: "
    //    << contract << L'\n';

    //
    // 1. Get the current process PEB.
    //
    auto peb = reinterpret_cast<const std::byte*>(
        __readgsqword(0x60)
        );

    //std::cout
    //    << "[APISET-DEBUG] PEB: 0x"
    //    << std::hex
    //    << reinterpret_cast<uintptr_t>(peb)
    //    << std::dec << '\n';

    if (peb == nullptr) {
        std::cout
            << "[APISET-ERROR] PEB pointer is null\n";

        return std::nullopt;
    }

    //
    // 2. Retrieve PEB->ApiSetMap.
    //
    constexpr size_t API_SET_MAP_OFFSET = 0x68;

    auto map = *reinterpret_cast<const ApiSetNamespace* const*>(
        peb + API_SET_MAP_OFFSET
        );

    //std::cout
    //    << "[APISET-DEBUG] ApiSetMap: 0x"
    //    << std::hex
    //    << reinterpret_cast<uintptr_t>(map)
    //    << std::dec << '\n';

    if (map == nullptr) {
        std::cout
            << "[APISET-ERROR] ApiSetMap pointer is null\n";

        return std::nullopt;
    }

    //
    // 3. Print namespace header.
    //
    //std::cout
    //    << "[APISET-DEBUG] Namespace header\n"
    //    << "    Version:     " << map->Version << '\n'
    //    << "    Size:        " << map->Size << '\n'
    //    << "    Flags:       0x"
    //    << std::hex << map->Flags << std::dec << '\n'
    //    << "    Count:       " << map->Count << '\n'
    //    << "    EntryOffset: 0x"
    //    << std::hex << map->EntryOffset << std::dec << '\n'
    //    << "    HashOffset:  0x"
    //    << std::hex << map->HashOffset << std::dec << '\n'
    //    << "    HashFactor:  " << map->HashFactor << '\n';

    //
    // Some basic sanity checks. If these fail, we're probably
    // reading the wrong PEB offset / structure layout.
    //
    if (map->Size < sizeof(ApiSetNamespace)) {
        std::cout
            << "[APISET-ERROR] Namespace size is invalid: "
            << map->Size << '\n';

        return std::nullopt;
    }

    if (map->Count == 0) {
        std::cout
            << "[APISET-ERROR] Namespace contains zero entries\n";

        return std::nullopt;
    }

    if (map->EntryOffset >= map->Size) {
        std::cout
            << "[APISET-ERROR] EntryOffset lies outside namespace\n";

        return std::nullopt;
    }

    //
    // 4. Normalize requested contract.
    //
    std::wstring target(contract);

    std::transform(
        target.begin(),
        target.end(),
        target.begin(),
        [](wchar_t c) {
            return static_cast<wchar_t>(std::towlower(c));
        }
    );

    if (target.ends_with(L".dll")) {
        target.resize(target.size() - 4);
    }

    //std::wcout
    //    << L"[APISET-DEBUG] Normalized target: "
    //    << target << L'\n';

    const auto* base =
        reinterpret_cast<const std::byte*>(map);

    const auto* entries =
        reinterpret_cast<const ApiSetNamespaceEntry*>(
            base + map->EntryOffset
            );
    //std::cout
    //    << "[APISET-DEBUG] First namespace entries:\n";

    for (ULONG i = 0; i < std::min<ULONG>(map->Count, 10); ++i) {

        const ApiSetNamespaceEntry& entry = entries[i];

        //std::cout
        //    << "    [" << i << "]"
        //    << " Flags=0x" << std::hex << entry.Flags
        //    << " NameOffset=0x" << entry.NameOffset
        //    << " NameLength=0x" << entry.NameLength
        //    << " HashedLength=0x" << entry.HashedLength
        //    << " ValueOffset=0x" << entry.ValueOffset
        //    << " ValueCount=0x" << entry.ValueCount
        //    << std::dec << '\n';

        uint64_t nameEnd =
            static_cast<uint64_t>(entry.NameOffset) +
            entry.NameLength;

        if (entry.NameLength != 0 &&
            nameEnd <= map->Size &&
            (entry.NameLength % sizeof(wchar_t)) == 0)
        {
            const auto* namePtr =
                reinterpret_cast<const wchar_t*>(
                    base + entry.NameOffset
                    );

            std::wstring_view name(
                namePtr,
                entry.NameLength / sizeof(wchar_t)
            );

            //std::wcout
            //    << L"         Name=\"" << name << L"\"\n";
        }
        else {
            std::cout
                << "         [invalid name range]\n";
        }
    }
    //
    // 5. Search namespace entries.
    //
    for (ULONG i = 0; i < map->Count; ++i) {

        const ApiSetNamespaceEntry& entry = entries[i];

        //
        // Validate the entry before dereferencing offsets.
        //
        uint64_t nameEnd =
            static_cast<uint64_t>(entry.NameOffset) +
            entry.NameLength;

        if (nameEnd > map->Size) {
            std::cout
                << "[APISET-ERROR] Entry " << i
                << " has invalid name range\n"
                << "    NameOffset: 0x"
                << std::hex << entry.NameOffset << '\n'
                << "    NameLength: 0x"
                << entry.NameLength << '\n'
                << "    MapSize:    0x"
                << map->Size
                << std::dec << '\n';

            return std::nullopt;
        }

        const auto* contractName =
            reinterpret_cast<const wchar_t*>(
                base + entry.NameOffset
                );

        std::wstring_view entryName(
            contractName,
            entry.NameLength / sizeof(wchar_t)
        );

        size_t hashedChars =
            entry.HashedLength / sizeof(wchar_t);

        if (hashedChars == 0 ||
            hashedChars > entryName.size() ||
            hashedChars > target.size())
        {
            continue;
        }

        std::wstring_view entryHashedName =
            entryName.substr(0, hashedChars);

        std::wstring_view targetHashedName(
            target.data(),
            hashedChars
        );

        if (entryHashedName != targetHashedName) {
            continue;
        }

        //std::wcout
        //    << L"[APISET-DEBUG] Found contract entry #"
        //    << i << L": " << entryName << L'\n';

        //std::cout
        //    << "    ValueOffset: 0x"
        //    << std::hex << entry.ValueOffset << std::dec << '\n'
        //    << "    ValueCount:  "
        //    << entry.ValueCount << '\n';

        if (entry.ValueCount == 0) {
            std::cout
                << "[APISET-ERROR] Contract has no value entries\n";

            return std::nullopt;
        }

        uint64_t valuesEnd =
            static_cast<uint64_t>(entry.ValueOffset) +
            static_cast<uint64_t>(entry.ValueCount) *
            sizeof(ApiSetValueEntry);

        if (valuesEnd > map->Size) {
            std::cout
                << "[APISET-ERROR] Value table lies outside namespace\n";

            return std::nullopt;
        }

        const auto* values = reinterpret_cast<const ApiSetValueEntry*>(base + entry.ValueOffset);

        //
        // 6. Select mapping for this contract.
        //
        // A contract can contain parent-specific mappings.
        // Prefer one matching parentModule. If none matches,
        // fall back to the unnamed/default mapping.
        //

        const ApiSetValueEntry* selected = nullptr;
        const ApiSetValueEntry* defaultValue = nullptr;

        std::wstring normalizedParent(parentModule);

        std::transform(
            normalizedParent.begin(),
            normalizedParent.end(),
            normalizedParent.begin(),
            [](wchar_t c) {
                return static_cast<wchar_t>(std::towlower(c));
            }
        );

        // Search every value. Remember the default, but don't
        // select it until we've checked for a parent-specific match.
        for (ULONG j = 0; j < entry.ValueCount; ++j) {

            const ApiSetValueEntry& value = values[j];

            //
            // Unnamed entry = default mapping.
            //
            if (value.NameLength == 0) {
                if (defaultValue == nullptr) {
                    defaultValue = &value;
                }

                continue;
            }

            //
            // No parent was supplied, so named mappings
            // cannot be selected.
            //
            if (normalizedParent.empty()) {
                continue;
            }

            uint64_t aliasEnd =
                static_cast<uint64_t>(value.NameOffset) +
                value.NameLength;

            if (aliasEnd > map->Size ||
                (value.NameLength % sizeof(wchar_t)) != 0)
            {
                std::cout
                    << "[APISET-ERROR] Alias lies outside namespace\n";

                return std::nullopt;
            }

            const auto* aliasPtr =
                reinterpret_cast<const wchar_t*>(
                    base + value.NameOffset
                    );

            std::wstring alias(
                aliasPtr,
                value.NameLength / sizeof(wchar_t)
            );

            std::transform(
                alias.begin(),
                alias.end(),
                alias.begin(),
                [](wchar_t c) {
                    return static_cast<wchar_t>(std::towlower(c));
                }
            );

            if (alias == normalizedParent) {

                selected = &value;

                //std::wcout
                //    << L"[APISET-DEBUG] Parent-specific mapping\n"
                //    << L"    parent: " << normalizedParent << L'\n'
                //    << L"    alias:  " << alias << L'\n';

                break;
            }
        }

        //
        // No parent-specific mapping matched.
        // Fall back to the unnamed/default mapping.
        //
        if (selected == nullptr) {
            selected = defaultValue;
        }

        if (selected == nullptr) {
            std::cout
                << "[APISET-ERROR] Contract found, "
                "but no applicable mapping was found\n";

            return std::nullopt;
        }

        if (selected->ValueLength == 0) {
            std::cout
                << "[APISET-ERROR] Selected mapping has empty host name\n";

            return std::nullopt;
        }

        uint64_t hostEnd =
            static_cast<uint64_t>(selected->ValueOffset) +
            selected->ValueLength;

        if (hostEnd > map->Size ||
            (selected->ValueLength % sizeof(wchar_t)) != 0)
        {
            std::cout
                << "[APISET-ERROR] Host lies outside namespace\n";

            return std::nullopt;
        }

        const auto* hostPtr =
            reinterpret_cast<const wchar_t*>(
                base + selected->ValueOffset
                );

        std::wstring host(
            hostPtr,
            selected->ValueLength / sizeof(wchar_t)
        );

        //std::wcout
        //    << L"[APISET-DEBUG] Resolved "
        //    << target
        //    << L" -> "
        //    << host
        //    << L'\n';

        return host;
 
    }

    std::wcout
        << L"[APISET-ERROR] Contract not found in namespace: "
        << target << L'\n';

    return std::nullopt;
}