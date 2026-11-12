#include "ApiSetResolver.h"
#include "pe/PEFile.h"
#include "LoaderConst.h"
#include "windows/WinStructs.h"

using win::IMAGE_SECTION_HEADER;
using win::API_SET_NAMESPACE;
using win::API_SET_NAMESPACE_ENTRY;
using win::API_SET_HASH_ENTRY;
using win::API_SET_VALUE_ENTRY;

using win::BYTE;
using win::ULONG;

#define API_SET_SCHEMA_ENTRY_FLAGS_SEALED 1

namespace emu {

    bool ApiSetResolver::initialize(const std::filesystem::path& schemaPath) {

        auto schemaOpt = PEFile::open(schemaPath);
        if (!schemaOpt.has_value()) {
            return false;
        }

        auto schema = *schemaOpt;

        // Iterate over sections
        for (uint16_t i = 0; i < schema.sections(); i++){

            auto section = schema.sectionHeader(i);

            if (section && std::memcmp(section->Name, ".apiset", 7) == 0 && (section->Name[7] == '\0')) {

                auto data = schema.data();
                size_t offset = section->PointerToRawData;
                size_t size = section->SizeOfRawData;

                if (offset > data.size() || size > data.size() - offset) {
                    return false;
                }

                std::span<const uint8_t> apiset = data.subspan(offset, size);
                apiset_.assign(apiset.begin(), apiset.end());
            }
        }

        return true;
    }

    std::optional<std::string> ApiSetResolver::resolve(const std::string& contract, const std::string& importingModule) const {
    
        std::span<const uint8_t> data(apiset_);

        auto header = readStruct<API_SET_NAMESPACE>(data, 0);
        if (!header || header->Version != 6 || header->Size < sizeof(API_SET_NAMESPACE) || header->Size > data.size()) {
            return std::nullopt;
        }
        
        data = data.first(header->Size);

        if (!validArray(header->EntryOffset, header->Count, sizeof(API_SET_NAMESPACE_ENTRY), data.size()) ||
            !validArray(header->HashOffset, header->Count, sizeof(API_SET_HASH_ENTRY), data.size())) {

            return std::nullopt;
        }

        std::string normalizedContract = lowerAscii(contract);

        if (normalizedContract.ends_with(".dll")) {
            normalizedContract.resize(normalizedContract.size() - 4);
        }


        // Remove version 
        size_t dash = contract.rfind('-');
        if (dash == std::string::npos || dash == 0 || dash + 1 == normalizedContract.size()) {
            return std::nullopt;
        }

        std::string hashedName = contract.substr(0, dash);

        uint32_t targetHash = 0;

        for (unsigned char c : hashedName) {
            targetHash = targetHash * header->HashFactor + c;
        }


        auto getHash = [&](size_t i) {
            return readStruct<API_SET_HASH_ENTRY>(data, header->HashOffset + i * sizeof(API_SET_HASH_ENTRY));
        };

        auto getEntry = [&](size_t i) {
            return readStruct<API_SET_NAMESPACE_ENTRY>(data, header->EntryOffset + i * sizeof(API_SET_NAMESPACE_ENTRY));
        };

        

        size_t low = 0;
        size_t high = header->Count;

        while (low < high) {
            size_t mid = low + (high - low) / 2;

            auto hashEntry = getHash(mid);
            if (!hashEntry) {
                return std::nullopt;
            }

            if (hashEntry->)
        }








    }



    // Helpers only used by ApiSetResolver
    namespace {

        bool validSchemaRange(size_t offset, size_t size, size_t total) {
            return offset <= total && size <= total - offset;
        }

        bool validArray(size_t offset, size_t count, size_t elementSize, size_t size) {
            return elementSize != 0 && offset <= size && count <= (size - offset) / elementSize;
        }

        template<typename T> std::optional<T> readStruct(std::span<const uint8_t> data, size_t offset) {
            
            if (!validRange(offset, sizeof(T), data.size())){
                return std::nullopt;
            }

            T result{};
            std::memcpy(&result, data.data() + offset, sizeof(T));
            return result;
        }

        std::optional<std::string> readUtf16(std::span<const uint8_t> data, uint32_t offset, uint32_t length) {
            
            if (length % 2 != 0 || !validSchemaRange(offset, length, data.size())) {
                return std::nullopt;
            }

            std::string result;
            result.reserve(length / 2);

            for (size_t i = 0; i < length; i += 2) {
                uint8_t lo = data[offset + i];
                uint8_t hi = data[offset + i + 1];

                if (hi != 0 || lo > 0x7F)
                    return std::nullopt;

                result.push_back(static_cast<char>(lo));
            }

            return result;
        }

        std::string lowerAscii(std::string_view name) {
            std::string result(name);

            for (char& c : result) {
                if (c >= 'A' && c <= 'Z')
                    c = static_cast<char>(c - 'A' + 'a');
            }

            return result;
        }
    }
}