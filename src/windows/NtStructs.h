#pragma once 

#include "common/Types.h"
#include "windows/WinTypes.h"

namespace win {

	struct LIST_ENTRY {
		uint64_t Flink;
		uint64_t Blink;
	};

	struct UNICODE_STRING {
		WORD Length;
		WORD MaximumLength;
		uint32_t Padding;
		uint64_t Buffer;
	};

}