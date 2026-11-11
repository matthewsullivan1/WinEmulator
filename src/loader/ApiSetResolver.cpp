#pragma once 

#include "ApiSetResolver.h"
#include "pe/PEFile.h"
#include "LoaderConst.h"

namespace emu {

    bool ApiSetResolver::initialize(const std::filesystem::path& schemaPath) {

        auto schema = PEFile::open(schemaPath);
        if (!schema.has_value()) {

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