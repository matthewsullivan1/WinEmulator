//#include "common/Types.h"
//#include "loader/Loader.h"
//#include "memory/MemoryManager.h"
//#include "runtime/Process.h"

#include "loader/ApiSetResolver.h"
#include "loader/Loader.h"

#include <array>
#include <string_view>
#include <iostream>

void testApiSetResolver(const emu::ApiSetResolver& resolver) {
    struct TestCase {
        std::string_view contract;
        std::string_view importer;
        bool expectSuccess;
    };

    constexpr std::array tests = {
        TestCase{"api-ms-win-core-file-l1-1-0.dll", "test.exe", true},
        TestCase{"api-ms-win-core-synch-l1-2-0.dll", "test.exe", true},
        TestCase{"api-ms-win-core-heap-l1-1-0.dll", "test.exe", true},
        TestCase{"api-ms-win-core-processthreads-l1-1-0.dll", "test.exe", true},
        TestCase{"api-ms-win-core-libraryloader-l1-2-0.dll", "test.exe", true},
        TestCase{"api-ms-win-this-contract-does-not-exist-l1-1-0.dll", "test.exe", false}
    };

    size_t passed = 0;

    for (const auto& test : tests) {
        auto result = resolver.resolve(std::string(test.contract), std::string(test.importer));
        bool success = result.has_value() == test.expectSuccess;

        std::cout << (success ? "[PASS] " : "[FAIL] ") << test.contract;

        if (result) std::cout << " -> " << *result;
        else std::cout << " -> unresolved";

        std::cout << '\n';

        if (success) ++passed;
    }

    std::cout << "\nPassed " << passed << "/" << tests.size() << " tests\n";
}

int main() {
    emu::Process p;
    const std::filesystem::path api = "guest/windows/system32/apisetschema.dll";
    const std::filesystem::path prog = "guest/prog.exe";

	emu::ApiSetResolver a;
    if (!a.initialize(api)) {
        return 1;
    }


    emu::Loader l(p, a);

	return 0;
}


/*
Load guest profile/schema
construct process
construct loader with process and guest environment
runtimeboostrap prepares process
	- request main image and ntldll mappings 
	- complete dependencies, relocations, imports
	- construct supported process/thread environment
	- finalize image protections


memory
- commit already committed page : succeeds for ordinary commitment within a valid reservation
- commit a mixed range : preserve existing pages and commit the missing pages
- decommit already reserved pages : succeeds for an otherwise valid range, those pages remain reserved
- query within an allocation : return the consective run beginning at the queried page with matching attributes
- query free address space : must be able to describe free ranges 
- explicit unaligned reservation : windows facing operation applies its address/range normalization rules




- Commit:
	- already committed : succeeds
	- mixed range of committed/reserved : preserve committed, commit reserved

- Decommit reserved pages:
	- Succeeds for the valid range of reserved pages

- Query
	- Return all pages w/ attributes, including those that are free

- Explicitly unaligned reservation ? handled by MM? 
*/