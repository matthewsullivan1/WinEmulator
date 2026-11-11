#pragma once 

#include "ApiSetResolver.h"
#include "pe/PEFile.h"
#include "LoaderConst.h"

using win::IMAGE_SECTION_HEADER;
using win::BYTE;

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

    }

    namespace {

        bool validSchemaRange(size_t offset, size_t size, size_t total) {
            return offset <= total && size <= total - offset;
        }

        struct ApiSetNamespaceV6 {
            uint32_t version;
            uint32_t size;
            uint32_t flags;
            uint32_t count;
            uint32_t entryOffset;
            uint32_t hashOffset;
            uint32_t hashFactor;
        };

        struct ApiSetNamespaceEntryV6 {
            uint32_t flags;
            uint32_t nameOffset;
            uint32_t nameLength;
            uint32_t hashedLength;
            uint32_t valueOffset;
            uint32_t valueCount;
        };

        struct ApiSetValueEntryV6 {
            uint32_t flags;
            uint32_t nameOffset;
            uint32_t nameLength;
            uint32_t valueOffset;
            uint32_t valueLength;
        };

    }
}